/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database.hpp"

#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>

#include "esp_log.h"
#include "ff.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"

#include "db_task.hpp"
#include "env_esp.hpp"
#include "file_gatherer.hpp"
#include "records.hpp"
#include "result.hpp"
#include "song.hpp"
#include "tag_parser.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
static const char* kTag = "DB";

static const char kSongIdKey[] = "next_song_id";

static std::atomic<bool> sIsDbOpen(false);

static FileGathererImpl sFileGatherer;
static TagParserImpl sTagParser;

template <typename Parser>
auto IterateAndParse(leveldb::Iterator* it, std::size_t limit, Parser p)
    -> void {
  for (int i = 0; i < limit; i++) {
    if (!it->Valid()) {
      delete it;
      break;
    }
    std::invoke(p, it->key(), it->value());
    it->Next();
  }
}

auto Database::Open() -> cpp::result<Database*, DatabaseError> {
  return Open(&sFileGatherer, &sTagParser);
}

auto Database::Open(IFileGatherer* gatherer, ITagParser* parser)
    -> cpp::result<Database*, DatabaseError> {
  // TODO(jacqueline): Why isn't compare_and_exchange_* available?
  if (sIsDbOpen.exchange(true)) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  if (!StartDbTask()) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  return RunOnDbTask<cpp::result<Database*, DatabaseError>>(
             [=]() -> cpp::result<Database*, DatabaseError> {
               leveldb::DB* db;
               leveldb::Cache* cache = leveldb::NewLRUCache(24 * 1024);
               leveldb::Options options;
               options.env = sEnv.env();
               options.create_if_missing = true;
               options.write_buffer_size = 48 * 1024;
               options.max_file_size = 32;
               options.block_cache = cache;
               options.block_size = 512;

               auto status = leveldb::DB::Open(options, "/.db", &db);
               if (!status.ok()) {
                 delete cache;
                 ESP_LOGE(kTag, "failed to open db, status %s",
                          status.ToString().c_str());
                 return cpp::fail(FAILED_TO_OPEN);
               }

               ESP_LOGI(kTag, "Database opened successfully");
               return new Database(db, cache, gatherer, parser);
             })
      .get();
}

auto Database::Destroy() -> void {
  leveldb::Options options;
  options.env = sEnv.env();
  leveldb::DestroyDB("/.db", options);
}

Database::Database(leveldb::DB* db,
                   leveldb::Cache* cache,
                   IFileGatherer* file_gatherer,
                   ITagParser* tag_parser)
    : db_(db),
      cache_(cache),
      file_gatherer_(file_gatherer),
      tag_parser_(tag_parser) {}

Database::~Database() {
  // Delete db_ first so that any outstanding background work finishes before
  // the background task is killed.
  delete db_;
  delete cache_;

  QuitDbTask();
  sIsDbOpen.store(false);
}

auto Database::Update() -> std::future<void> {
  return RunOnDbTask<void>([&]() -> void {
    // Stage 1: verify all existing songs are still valid.
    ESP_LOGI(kTag, "verifying existing songs");
    const leveldb::Snapshot* snapshot = db_->GetSnapshot();
    leveldb::ReadOptions read_options;
    read_options.fill_cache = false;
    read_options.snapshot = snapshot;
    leveldb::Iterator* it = db_->NewIterator(read_options);
    OwningSlice prefix = CreateDataPrefix();
    it->Seek(prefix.slice);
    while (it->Valid() && it->key().starts_with(prefix.slice)) {
      std::optional<SongData> song = ParseDataValue(it->value());
      if (!song) {
        // The value was malformed. Drop this record.
        ESP_LOGW(kTag, "dropping malformed metadata");
        db_->Delete(leveldb::WriteOptions(), it->key());
        it->Next();
        continue;
      }

      if (song->is_tombstoned()) {
        ESP_LOGW(kTag, "skipping tombstoned %lx", song->id());
        it->Next();
        continue;
      }

      SongTags tags;
      if (!tag_parser_->ReadAndParseTags(song->filepath(), &tags) ||
          tags.encoding == Encoding::kUnsupported) {
        // We couldn't read the tags for this song. Either they were
        // malformed, or perhaps the file is missing. Either way, tombstone
        // this record.
        ESP_LOGW(kTag, "entombing missing #%lx", song->id());
        dbPutSongData(song->Entomb());
        it->Next();
        continue;
      }

      uint64_t new_hash = tags.Hash();
      if (new_hash != song->tags_hash()) {
        // This song's tags have changed. Since the filepath is exactly the
        // same, we assume this is a legitimate correction. Update the
        // database.
        ESP_LOGI(kTag, "updating hash (%llx -> %llx)", song->tags_hash(),
                 new_hash);
        dbPutSongData(song->UpdateHash(new_hash));
        dbPutHash(new_hash, song->id());
      }

      it->Next();
    }
    delete it;
    db_->ReleaseSnapshot(snapshot);

    // Stage 2: search for newly added files.
    ESP_LOGI(kTag, "scanning for new songs");
    file_gatherer_->FindFiles("", [&](const std::string& path) {
      SongTags tags;
      if (!tag_parser_->ReadAndParseTags(path, &tags) ||
          tags.encoding == Encoding::kUnsupported) {
        // No parseable tags; skip this fiile.
        return;
      }

      // Check for any existing record with the same hash.
      uint64_t hash = tags.Hash();
      OwningSlice key = CreateHashKey(hash);
      std::optional<SongId> existing_hash;
      std::string raw_entry;
      if (db_->Get(leveldb::ReadOptions(), key.slice, &raw_entry).ok()) {
        existing_hash = ParseHashValue(raw_entry);
      }

      if (!existing_hash) {
        // We've never met this song before! Or we have, but the entry is
        // malformed. Either way, record this as a new song.
        SongId id = dbMintNewSongId();
        ESP_LOGI(kTag, "recording new 0x%lx", id);
        dbPutSong(id, path, hash);
        return;
      }

      std::optional<SongData> existing_data = dbGetSongData(*existing_hash);
      if (!existing_data) {
        // We found a hash that matches, but there's no data record? Weird.
        SongData new_data(*existing_hash, path, hash);
        dbPutSongData(new_data);
        return;
      }

      if (existing_data->is_tombstoned()) {
        ESP_LOGI(kTag, "exhuming song %lu", existing_data->id());
        dbPutSongData(existing_data->Exhume(path));
      } else if (existing_data->filepath() != path) {
        ESP_LOGW(kTag, "tag hash collision");
      }
    });
  });
}

auto Database::GetSongs(std::size_t page_size) -> std::future<Result<Song>*> {
  return RunOnDbTask<Result<Song>*>([=, this]() -> Result<Song>* {
    Continuation<Song> c{.iterator = nullptr,
                         .prefix = CreateDataPrefix().data,
                         .start_key = CreateDataPrefix().data,
                         .forward = true,
                         .was_prev_forward = true,
                         .page_size = page_size};
    return dbGetPage(c);
  });
}

auto Database::GetDump(std::size_t page_size)
    -> std::future<Result<std::string>*> {
  return RunOnDbTask<Result<std::string>*>([=, this]() -> Result<std::string>* {
    Continuation<std::string> c{.iterator = nullptr,
                                .prefix = "",
                                .start_key = "",
                                .forward = true,
                                .was_prev_forward = true,
                                .page_size = page_size};
    return dbGetPage(c);
  });
}

template <typename T>
auto Database::GetPage(Continuation<T>* c) -> std::future<Result<T>*> {
  Continuation<T> copy = *c;
  return RunOnDbTask<Result<T>*>(
      [=, this]() -> Result<T>* { return dbGetPage(copy); });
}

template auto Database::GetPage<Song>(Continuation<Song>* c)
    -> std::future<Result<Song>*>;
template auto Database::GetPage<std::string>(Continuation<std::string>* c)
    -> std::future<Result<std::string>*>;

auto Database::dbMintNewSongId() -> SongId {
  SongId next_id = 1;
  std::string val;
  auto status = db_->Get(leveldb::ReadOptions(), kSongIdKey, &val);
  if (status.ok()) {
    next_id = BytesToSongId(val).value_or(next_id);
  } else if (!status.IsNotFound()) {
    // TODO(jacqueline): Handle this more.
    ESP_LOGE(kTag, "failed to get next song id");
  }

  if (!db_->Put(leveldb::WriteOptions(), kSongIdKey,
                SongIdToBytes(next_id + 1).slice)
           .ok()) {
    ESP_LOGE(kTag, "failed to write next song id");
  }

  return next_id;
}

auto Database::dbEntomb(SongId id, uint64_t hash) -> void {
  OwningSlice key = CreateHashKey(hash);
  OwningSlice val = CreateHashValue(id);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to entomb #%llx (id #%lx)", hash, id);
  }
}

auto Database::dbPutSongData(const SongData& s) -> void {
  OwningSlice key = CreateDataKey(s.id());
  OwningSlice val = CreateDataValue(s);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to write data for #%lx", s.id());
  }
}

auto Database::dbGetSongData(SongId id) -> std::optional<SongData> {
  OwningSlice key = CreateDataKey(id);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key.slice, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for #%lx", id);
    return {};
  }
  return ParseDataValue(raw_val);
}

auto Database::dbPutHash(const uint64_t& hash, SongId i) -> void {
  OwningSlice key = CreateHashKey(hash);
  OwningSlice val = CreateHashValue(i);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to write hash for #%lx", i);
  }
}

auto Database::dbGetHash(const uint64_t& hash) -> std::optional<SongId> {
  OwningSlice key = CreateHashKey(hash);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key.slice, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for hash #%llx", hash);
    return {};
  }
  return ParseHashValue(raw_val);
}

auto Database::dbPutSong(SongId id,
                         const std::string& path,
                         const uint64_t& hash) -> void {
  dbPutSongData(SongData(id, path, hash));
  dbPutHash(hash, id);
}

template <typename T>
auto Database::dbGetPage(const Continuation<T>& c) -> Result<T>* {
  // Work out our starting point. Sometimes this will already done.
  leveldb::Iterator* it = nullptr;
  if (c.iterator != nullptr) {
    it = c.iterator->release();
  }
  if (it == nullptr) {
    it = db_->NewIterator(leveldb::ReadOptions());
    it->Seek(c.start_key);
  }

  // Fix off-by-one if we just changed direction.
  if (c.forward != c.was_prev_forward) {
    if (c.forward) {
      it->Next();
    } else {
      it->Prev();
    }
  }

  // Grab results.
  std::optional<std::string> first_key;
  std::vector<T> records;
  while (records.size() < c.page_size && it->Valid()) {
    if (!it->key().starts_with(c.prefix)) {
      break;
    }
    if (!first_key) {
      first_key = it->key().ToString();
    }
    std::optional<T> parsed = ParseRecord<T>(it->key(), it->value());
    if (parsed) {
      records.push_back(*parsed);
    }
    if (c.forward) {
      it->Next();
    } else {
      it->Prev();
    }
  }

  std::unique_ptr<leveldb::Iterator> iterator(it);
  if (iterator != nullptr) {
    if (!iterator->Valid() || !it->key().starts_with(c.prefix)) {
      iterator.reset();
    }
  }

  // Put results into canonical order if we were iterating backwards.
  if (!c.forward) {
    std::reverse(records.begin(), records.end());
  }

  // Work out the new continuations.
  std::optional<Continuation<T>> next_page;
  if (c.forward) {
    if (iterator != nullptr) {
      // We were going forward, and now we want the next page. Re-use the
      // existing iterator, and point the start key at it.
      std::string key = iterator->key().ToString();
      next_page = Continuation<T>{
          .iterator = std::make_shared<std::unique_ptr<leveldb::Iterator>>(
              std::move(iterator)),
          .prefix = c.prefix,
          .start_key = key,
          .forward = true,
          .was_prev_forward = true,
          .page_size = c.page_size,
      };
    }
    // No iterator means we ran out of results in this direction.
  } else {
    // We were going backwards, and now we want the next page. This is a
    // reversal, to set the start key to the first record we saw and mark that
    // it's off by one.
    next_page = Continuation<T>{
        .iterator = nullptr,
        .prefix = c.prefix,
        .start_key = *first_key,
        .forward = true,
        .was_prev_forward = false,
        .page_size = c.page_size,
    };
  }

  std::optional<Continuation<T>> prev_page;
  if (c.forward) {
    // We were going forwards, and now we want the previous page. Set the search
    // key to the first result we saw, and mark that it's off by one.
    prev_page = Continuation<T>{
        .iterator = nullptr,
        .prefix = c.prefix,
        .start_key = *first_key,
        .forward = false,
        .was_prev_forward = true,
        .page_size = c.page_size,
    };
  } else {
    if (iterator != nullptr) {
      // We were going backwards, and we still want to go backwards. The
      // iterator is still valid.
      std::string key = iterator->key().ToString();
      prev_page = Continuation<T>{
          .iterator = std::make_shared<std::unique_ptr<leveldb::Iterator>>(
              std::move(iterator)),
          .prefix = c.prefix,
          .start_key = key,
          .forward = false,
          .was_prev_forward = false,
          .page_size = c.page_size,
      };
    }
    // No iterator means we ran out of results in this direction.
  }

  return new Result<T>(std::move(records), next_page, prev_page);
}

template auto Database::dbGetPage<Song>(const Continuation<Song>& c)
    -> Result<Song>*;
template auto Database::dbGetPage<std::string>(
    const Continuation<std::string>& c) -> Result<std::string>*;

template <>
auto Database::ParseRecord<Song>(const leveldb::Slice& key,
                                 const leveldb::Slice& val)
    -> std::optional<Song> {
  std::optional<SongData> data = ParseDataValue(val);
  if (!data || data->is_tombstoned()) {
    return {};
  }
  SongTags tags;
  if (!tag_parser_->ReadAndParseTags(data->filepath(), &tags)) {
    return {};
  }
  return Song(*data, tags);
}

template <>
auto Database::ParseRecord<std::string>(const leveldb::Slice& key,
                                        const leveldb::Slice& val)
    -> std::optional<std::string> {
  std::ostringstream stream;
  stream << "key: ";
  if (key.size() < 3 || key.data()[1] != '\0') {
    stream << key.ToString().c_str();
  } else {
    std::string str = key.ToString();
    for (size_t i = 0; i < str.size(); i++) {
      if (i == 0) {
        stream << str[i];
      } else if (i == 1) {
        stream << " / 0x";
      } else {
        stream << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(str[i]);
      }
    }
  }
  stream << "\tval: 0x";
  std::string str = val.ToString();
  for (int i = 0; i < val.size(); i++) {
    stream << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(str[i]);
  }
  return stream.str();
}

}  // namespace database
