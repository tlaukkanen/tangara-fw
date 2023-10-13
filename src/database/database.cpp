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
#include <optional>
#include <sstream>

#include "esp_log.h"
#include "ff.h"
#include "freertos/projdefs.h"
#include "index.hpp"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"

#include "db_events.hpp"
#include "env_esp.hpp"
#include "event_queue.hpp"
#include "file_gatherer.hpp"
#include "memory_resource.hpp"
#include "records.hpp"
#include "result.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "track.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
static const char* kTag = "DB";

static const char kTrackIdKey[] = "next_track_id";

static std::atomic<bool> sIsDbOpen(false);

auto Database::Open(IFileGatherer& gatherer, ITagParser& parser)
    -> cpp::result<Database*, DatabaseError> {
  // TODO(jacqueline): Why isn't compare_and_exchange_* available?
  if (sIsDbOpen.exchange(true)) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  if (!leveldb::sBackgroundThread) {
    leveldb::sBackgroundThread.reset(
        tasks::Worker::Start<tasks::Type::kDatabaseBackground>());
  }

  std::shared_ptr<tasks::Worker> worker(
      tasks::Worker::Start<tasks::Type::kDatabase>());
  return worker
      ->Dispatch<cpp::result<Database*, DatabaseError>>(
          [&]() -> cpp::result<Database*, DatabaseError> {
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
            return new Database(db, cache, gatherer, parser, worker);
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
                   IFileGatherer& file_gatherer,
                   ITagParser& tag_parser,
                   std::shared_ptr<tasks::Worker> worker)
    : db_(db),
      cache_(cache),
      worker_task_(worker),
      file_gatherer_(file_gatherer),
      tag_parser_(tag_parser) {}

Database::~Database() {
  // Delete db_ first so that any outstanding background work finishes before
  // the background task is killed.
  delete db_;
  delete cache_;

  sIsDbOpen.store(false);
}

auto Database::Update() -> std::future<void> {
  events::Ui().Dispatch(event::UpdateStarted{});
  return worker_task_->Dispatch<void>([&]() -> void {
    leveldb::ReadOptions read_options;
    read_options.fill_cache = false;

    // Stage 0: discard indexes
    // TODO(jacqueline): I think it should be possible to incrementally update
    // indexes, but my brain hurts.
    ESP_LOGI(kTag, "dropping stale indexes");
    {
      std::unique_ptr<leveldb::Iterator> it{db_->NewIterator(read_options)};
      OwningSlice prefix = EncodeAllIndexesPrefix();
      it->Seek(prefix.slice);
      while (it->Valid() && it->key().starts_with(prefix.slice)) {
        db_->Delete(leveldb::WriteOptions(), it->key());
        it->Next();
      }
    }

    // Stage 1: verify all existing tracks are still valid.
    ESP_LOGI(kTag, "verifying existing tracks");
    {
      uint64_t num_processed = 0;
      std::unique_ptr<leveldb::Iterator> it{db_->NewIterator(read_options)};
      OwningSlice prefix = EncodeDataPrefix();
      it->Seek(prefix.slice);
      while (it->Valid() && it->key().starts_with(prefix.slice)) {
        num_processed++;
        events::Ui().Dispatch(event::UpdateProgress{
            .stage = event::UpdateProgress::Stage::kVerifyingExistingTracks,
            .val = num_processed,
        });

        std::shared_ptr<TrackData> track = ParseDataValue(it->value());
        if (!track) {
          // The value was malformed. Drop this record.
          ESP_LOGW(kTag, "dropping malformed metadata");
          db_->Delete(leveldb::WriteOptions(), it->key());
          it->Next();
          continue;
        }

        if (track->is_tombstoned()) {
          ESP_LOGW(kTag, "skipping tombstoned %lx", track->id());
          it->Next();
          continue;
        }

        std::shared_ptr<TrackTags> tags =
            tag_parser_.ReadAndParseTags(track->filepath());
        if (!tags || tags->encoding() == Container::kUnsupported) {
          // We couldn't read the tags for this track. Either they were
          // malformed, or perhaps the file is missing. Either way, tombstone
          // this record.
          ESP_LOGW(kTag, "entombing missing #%lx", track->id());
          dbPutTrackData(track->Entomb());
          it->Next();
          continue;
        }

        // At this point, we know that the track still exists in its original
        // location. All that's left to do is update any metadata about it.

        uint64_t new_hash = tags->Hash();
        if (new_hash != track->tags_hash()) {
          // This track's tags have changed. Since the filepath is exactly the
          // same, we assume this is a legitimate correction. Update the
          // database.
          ESP_LOGI(kTag, "updating hash (%llx -> %llx)", track->tags_hash(),
                   new_hash);
          dbPutTrackData(track->UpdateHash(new_hash));
          dbPutHash(new_hash, track->id());
        }

        Track t{track, tags};

        dbCreateIndexesForTrack(t);

        it->Next();
      }
    }

    // Stage 2: search for newly added files.
    ESP_LOGI(kTag, "scanning for new tracks");
    uint64_t num_processed = 0;
    file_gatherer_.FindFiles("", [&](const std::pmr::string& path) {
      num_processed++;
      events::Ui().Dispatch(event::UpdateProgress{
          .stage = event::UpdateProgress::Stage::kScanningForNewTracks,
          .val = num_processed,
      });

      std::shared_ptr<TrackTags> tags = tag_parser_.ReadAndParseTags(path);
      if (!tags || tags->encoding() == Container::kUnsupported) {
        // No parseable tags; skip this fiile.
        return;
      }

      // Check for any existing record with the same hash.
      uint64_t hash = tags->Hash();
      OwningSlice key = EncodeHashKey(hash);
      std::optional<TrackId> existing_hash;
      std::string raw_entry;
      if (db_->Get(leveldb::ReadOptions(), key.slice, &raw_entry).ok()) {
        existing_hash = ParseHashValue(raw_entry);
      }

      if (!existing_hash) {
        // We've never met this track before! Or we have, but the entry is
        // malformed. Either way, record this as a new track.
        TrackId id = dbMintNewTrackId();
        ESP_LOGI(kTag, "recording new 0x%lx", id);

        auto data = std::make_shared<TrackData>(id, path, hash);
        dbPutTrackData(*data);
        dbPutHash(hash, id);
        auto t = std::make_shared<Track>(data, tags);
        dbCreateIndexesForTrack(*t);
        return;
      }

      std::shared_ptr<TrackData> existing_data = dbGetTrackData(*existing_hash);
      if (!existing_data) {
        // We found a hash that matches, but there's no data record? Weird.
        auto new_data = std::make_shared<TrackData>(*existing_hash, path, hash);
        dbPutTrackData(*new_data);
        auto t = std::make_shared<Track>(new_data, tags);
        dbCreateIndexesForTrack(*t);
        return;
      }

      if (existing_data->is_tombstoned()) {
        ESP_LOGI(kTag, "exhuming track %lu", existing_data->id());
        dbPutTrackData(existing_data->Exhume(path));
        auto t = std::make_shared<Track>(existing_data, tags);
        dbCreateIndexesForTrack(*t);
      } else if (existing_data->filepath() != path) {
        ESP_LOGW(kTag, "tag hash collision for %s and %s",
                 existing_data->filepath().c_str(), path.c_str());
        ESP_LOGI(kTag, "hash components: %s, %s, %s",
                 tags->at(Tag::kTitle).value_or("no title").c_str(),
                 tags->at(Tag::kArtist).value_or("no artist").c_str(),
                 tags->at(Tag::kAlbum).value_or("no album").c_str());
      }
    });
    events::Ui().Dispatch(event::UpdateFinished{});
  });
}

auto Database::GetTrackPath(TrackId id)
    -> std::future<std::optional<std::pmr::string>> {
  return worker_task_->Dispatch<std::optional<std::pmr::string>>(
      [=, this]() -> std::optional<std::pmr::string> {
        auto track_data = dbGetTrackData(id);
        if (track_data) {
          return track_data->filepath();
        }
        return {};
      });
}

auto Database::GetTrack(TrackId id) -> std::future<std::shared_ptr<Track>> {
  return worker_task_->Dispatch<std::shared_ptr<Track>>(
      [=, this]() -> std::shared_ptr<Track> {
        std::shared_ptr<TrackData> data = dbGetTrackData(id);
        if (!data || data->is_tombstoned()) {
          return {};
        }
        std::shared_ptr<TrackTags> tags =
            tag_parser_.ReadAndParseTags(data->filepath());
        if (!tags) {
          return {};
        }
        return std::make_shared<Track>(data, tags);
      });
}

auto Database::GetBulkTracks(std::vector<TrackId> ids)
    -> std::future<std::vector<std::shared_ptr<Track>>> {
  return worker_task_->Dispatch<std::vector<std::shared_ptr<Track>>>(
      [=, this]() -> std::vector<std::shared_ptr<Track>> {
        std::map<TrackId, std::shared_ptr<Track>> id_to_track{};

        // Sort the list of ids so that we can retrieve them all in a single
        // iteration through the database, without re-seeking.
        std::vector<TrackId> sorted_ids = ids;
        std::sort(sorted_ids.begin(), sorted_ids.end());

        std::unique_ptr<leveldb::Iterator> it{
            db_->NewIterator(leveldb::ReadOptions{})};
        for (const TrackId& id : sorted_ids) {
          OwningSlice key = EncodeDataKey(id);
          it->Seek(key.slice);
          if (!it->Valid() || it->key() != key.slice) {
            // This id wasn't found at all. Skip it.
            continue;
          }
          std::shared_ptr<Track> track =
              ParseRecord<Track>(it->key(), it->value());
          if (track) {
            id_to_track.insert({id, track});
          }
        }

        // We've fetched all of the ids in the request, so now just put them
        // back into the order they were asked for in.
        std::vector<std::shared_ptr<Track>> results;
        for (const TrackId& id : ids) {
          if (id_to_track.contains(id)) {
            results.push_back(id_to_track.at(id));
          } else {
            // This lookup failed.
            results.push_back({});
          }
        }
        return results;
      });
}

auto Database::GetIndexes() -> std::vector<IndexInfo> {
  // TODO(jacqueline): This probably needs to be async? When we have runtime
  // configurable indexes, they will need to come from somewhere.
  return {
      kAllTracks,
      kAllAlbums,
      kAlbumsByArtist,
      kTracksByGenre,
  };
}

auto Database::GetTracksByIndex(const IndexInfo& index, std::size_t page_size)
    -> std::future<Result<IndexRecord>*> {
  return worker_task_->Dispatch<Result<IndexRecord>*>(
      [=, this]() -> Result<IndexRecord>* {
        IndexKey::Header header{
            .id = index.id,
            .depth = 0,
            .components_hash = 0,
        };
        OwningSlice prefix = EncodeIndexPrefix(header);
        Continuation c{.prefix = prefix.data,
                       .start_key = prefix.data,
                       .forward = true,
                       .was_prev_forward = true,
                       .page_size = page_size};
        return dbGetPage<IndexRecord>(c);
      });
}

auto Database::GetTracks(std::size_t page_size) -> std::future<Result<Track>*> {
  return worker_task_->Dispatch<Result<Track>*>([=, this]() -> Result<Track>* {
    Continuation c{.prefix = EncodeDataPrefix().data,
                   .start_key = EncodeDataPrefix().data,
                   .forward = true,
                   .was_prev_forward = true,
                   .page_size = page_size};
    return dbGetPage<Track>(c);
  });
}

auto Database::GetDump(std::size_t page_size)
    -> std::future<Result<std::pmr::string>*> {
  return worker_task_->Dispatch<Result<std::pmr::string>*>(
      [=, this]() -> Result<std::pmr::string>* {
        Continuation c{.prefix = "",
                       .start_key = "",
                       .forward = true,
                       .was_prev_forward = true,
                       .page_size = page_size};
        return dbGetPage<std::pmr::string>(c);
      });
}

template <typename T>
auto Database::GetPage(Continuation* c) -> std::future<Result<T>*> {
  Continuation copy = *c;
  return worker_task_->Dispatch<Result<T>*>(
      [=, this]() -> Result<T>* { return dbGetPage<T>(copy); });
}

template auto Database::GetPage<Track>(Continuation* c)
    -> std::future<Result<Track>*>;
template auto Database::GetPage<IndexRecord>(Continuation* c)
    -> std::future<Result<IndexRecord>*>;
template auto Database::GetPage<std::pmr::string>(Continuation* c)
    -> std::future<Result<std::pmr::string>*>;

auto Database::dbMintNewTrackId() -> TrackId {
  TrackId next_id = 1;
  std::string val;
  auto status = db_->Get(leveldb::ReadOptions(), kTrackIdKey, &val);
  if (status.ok()) {
    next_id = BytesToTrackId(val).value_or(next_id);
  } else if (!status.IsNotFound()) {
    // TODO(jacqueline): Handle this more.
    ESP_LOGE(kTag, "failed to get next track id");
  }

  if (!db_->Put(leveldb::WriteOptions(), kTrackIdKey,
                TrackIdToBytes(next_id + 1).slice)
           .ok()) {
    ESP_LOGE(kTag, "failed to write next track id");
  }

  return next_id;
}

auto Database::dbEntomb(TrackId id, uint64_t hash) -> void {
  OwningSlice key = EncodeHashKey(hash);
  OwningSlice val = EncodeHashValue(id);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to entomb #%llx (id #%lx)", hash, id);
  }
}

auto Database::dbPutTrackData(const TrackData& s) -> void {
  OwningSlice key = EncodeDataKey(s.id());
  OwningSlice val = EncodeDataValue(s);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to write data for #%lx", s.id());
  }
}

auto Database::dbGetTrackData(TrackId id) -> std::shared_ptr<TrackData> {
  OwningSlice key = EncodeDataKey(id);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key.slice, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for #%lx", id);
    return {};
  }
  return ParseDataValue(raw_val);
}

auto Database::dbPutHash(const uint64_t& hash, TrackId i) -> void {
  OwningSlice key = EncodeHashKey(hash);
  OwningSlice val = EncodeHashValue(i);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to write hash for #%lx", i);
  }
}

auto Database::dbGetHash(const uint64_t& hash) -> std::optional<TrackId> {
  OwningSlice key = EncodeHashKey(hash);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key.slice, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for hash #%llx", hash);
    return {};
  }
  return ParseHashValue(raw_val);
}

auto Database::dbCreateIndexesForTrack(const Track& track) -> void {
  for (const IndexInfo& index : GetIndexes()) {
    leveldb::WriteBatch writes;
    if (Index(index, track, &writes)) {
      db_->Write(leveldb::WriteOptions(), &writes);
    }
  }
}

template <typename T>
auto Database::dbGetPage(const Continuation& c) -> Result<T>* {
  // Work out our starting point. Sometimes this will already done.
  std::unique_ptr<leveldb::Iterator> it{
      db_->NewIterator(leveldb::ReadOptions{})};
  it->Seek({c.start_key.data(), c.start_key.size()});

  // Fix off-by-one if we just changed direction.
  if (c.forward != c.was_prev_forward) {
    if (c.forward) {
      it->Next();
    } else {
      it->Prev();
    }
  }

  // Grab results.
  std::optional<std::pmr::string> first_key;
  std::vector<std::shared_ptr<T>> records;
  while (records.size() < c.page_size && it->Valid()) {
    if (!it->key().starts_with({c.prefix.data(), c.prefix.size()})) {
      break;
    }
    if (!first_key) {
      first_key = it->key().ToString();
    }
    std::shared_ptr<T> parsed = ParseRecord<T>(it->key(), it->value());
    if (parsed) {
      records.push_back(parsed);
    }
    if (c.forward) {
      it->Next();
    } else {
      it->Prev();
    }
  }

  if (!it->Valid() ||
      !it->key().starts_with({c.prefix.data(), c.prefix.size()})) {
    it.reset();
  }

  // Put results into canonical order if we were iterating backwards.
  if (!c.forward) {
    std::reverse(records.begin(), records.end());
  }

  // Work out the new continuations.
  std::optional<Continuation> next_page;
  if (c.forward) {
    if (it != nullptr) {
      // We were going forward, and now we want the next page.
      std::pmr::string key{it->key().data(), it->key().size(),
                           &memory::kSpiRamResource};
      next_page = Continuation{
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
    next_page = Continuation{
        .prefix = c.prefix,
        .start_key = *first_key,
        .forward = true,
        .was_prev_forward = false,
        .page_size = c.page_size,
    };
  }

  std::optional<Continuation> prev_page;
  if (c.forward) {
    // We were going forwards, and now we want the previous page. Set the search
    // key to the first result we saw, and mark that it's off by one.
    prev_page = Continuation{
        .prefix = c.prefix,
        .start_key = *first_key,
        .forward = false,
        .was_prev_forward = true,
        .page_size = c.page_size,
    };
  } else {
    if (it != nullptr) {
      // We were going backwards, and we still want to go backwards.
      std::pmr::string key{it->key().data(), it->key().size(),
                           &memory::kSpiRamResource};
      prev_page = Continuation{
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

template auto Database::dbGetPage<Track>(const Continuation& c)
    -> Result<Track>*;
template auto Database::dbGetPage<std::pmr::string>(const Continuation& c)
    -> Result<std::pmr::string>*;

template <>
auto Database::ParseRecord<IndexRecord>(const leveldb::Slice& key,
                                        const leveldb::Slice& val)
    -> std::shared_ptr<IndexRecord> {
  std::optional<IndexKey> data = ParseIndexKey(key);
  if (!data) {
    return {};
  }

  std::optional<std::pmr::string> title;
  if (!val.empty()) {
    title = val.ToString();
  }

  return std::make_shared<IndexRecord>(*data, title, data->track);
}

template <>
auto Database::ParseRecord<Track>(const leveldb::Slice& key,
                                  const leveldb::Slice& val)
    -> std::shared_ptr<Track> {
  std::shared_ptr<TrackData> data = ParseDataValue(val);
  if (!data || data->is_tombstoned()) {
    return {};
  }
  std::shared_ptr<TrackTags> tags =
      tag_parser_.ReadAndParseTags(data->filepath());
  if (!tags) {
    return {};
  }
  return std::make_shared<Track>(data, tags);
}

template <>
auto Database::ParseRecord<std::pmr::string>(const leveldb::Slice& key,
                                             const leveldb::Slice& val)
    -> std::shared_ptr<std::pmr::string> {
  std::ostringstream stream;
  stream << "key: ";
  if (key.size() < 3 || key.data()[1] != '\0') {
    stream << key.ToString().c_str();
  } else {
    for (size_t i = 0; i < key.size(); i++) {
      if (i == 0) {
        stream << key.data()[i];
      } else if (i == 1) {
        stream << " / 0x";
      } else {
        stream << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(key.data()[i]);
      }
    }
  }
  if (!val.empty()) {
    stream << "\tval: 0x";
    for (int i = 0; i < val.size(); i++) {
      stream << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<int>(val.data()[i]);
    }
  }
  std::pmr::string res{stream.str(), &memory::kSpiRamResource};
  return std::make_shared<std::pmr::string>(res);
}

IndexRecord::IndexRecord(const IndexKey& key,
                         std::optional<std::pmr::string> title,
                         std::optional<TrackId> track)
    : key_(key), override_text_(title), track_(track) {}

auto IndexRecord::text() const -> std::optional<std::pmr::string> {
  if (override_text_) {
    return override_text_;
  }
  return key_.item;
}

auto IndexRecord::track() const -> std::optional<TrackId> {
  return track_;
}

auto IndexRecord::Expand(std::size_t page_size) const
    -> std::optional<Continuation> {
  if (track_) {
    return {};
  }
  IndexKey::Header new_header = ExpandHeader(key_.header, key_.item);
  OwningSlice new_prefix = EncodeIndexPrefix(new_header);
  return Continuation{
      .prefix = new_prefix.data,
      .start_key = new_prefix.data,
      .forward = true,
      .was_prev_forward = true,
      .page_size = page_size,
  };
}

}  // namespace database
