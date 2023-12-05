/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database.hpp"

#include <stdint.h>
#include <sys/_stdint.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>

#include "collation.hpp"
#include "cppbor.h"
#include "esp_log.h"
#include "ff.h"
#include "freertos/projdefs.h"
#include "index.hpp"
#include "komihash.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/write_batch.h"

#include "db_events.hpp"
#include "env_esp.hpp"
#include "event_queue.hpp"
#include "file_gatherer.hpp"
#include "memory_resource.hpp"
#include "records.hpp"
#include "result.hpp"
#include "spi.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "track.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
[[maybe_unused]] static const char* kTag = "DB";

static const char kDbPath[] = "/.tangara-db";

static const char kKeyDbVersion[] = "schema_version";
static const uint8_t kCurrentDbVersion = 3;

static const char kKeyCustom[] = "U\0";
static const char kKeyCollator[] = "collator";
static const char kKeyTrackId[] = "next_track_id";

static std::atomic<bool> sIsDbOpen(false);

static auto CreateNewDatabase(leveldb::Options& options, locale::ICollator& col)
    -> leveldb::DB* {
  Database::Destroy();
  leveldb::DB* db;
  options.create_if_missing = true;
  auto status = leveldb::DB::Open(options, kDbPath, &db);
  if (!status.ok()) {
    ESP_LOGE(kTag, "failed to open db, status %s", status.ToString().c_str());
    return nullptr;
  }
  auto version_str = std::to_string(kCurrentDbVersion);
  status = db->Put(leveldb::WriteOptions{}, kKeyDbVersion, version_str);
  if (!status.ok()) {
    delete db;
    return nullptr;
  }
  ESP_LOGI(kTag, "opening db with collator %s",
           col.Describe().value_or("NULL").c_str());
  status = db->Put(leveldb::WriteOptions{}, kKeyCollator,
                   col.Describe().value_or(""));
  if (!status.ok()) {
    delete db;
    return nullptr;
  }
  return db;
}

static auto CheckDatabase(leveldb::DB& db, locale::ICollator& col) -> bool {
  leveldb::Status status;

  std::string raw_version;
  std::optional<uint8_t> version{};
  status = db.Get(leveldb::ReadOptions{}, kKeyDbVersion, &raw_version);
  if (status.ok()) {
    version = std::stoi(raw_version);
  }
  if (!version || *version != kCurrentDbVersion) {
    ESP_LOGW(kTag, "db version missing or incorrect");
    return false;
  }

  std::string collator;
  status = db.Get(leveldb::ReadOptions{}, kKeyCollator, &collator);
  if (!status.ok()) {
    ESP_LOGW(kTag, "db collator is unknown");
    return false;
  }
  auto needed = col.Describe();

  if ((needed && needed.value() != collator) ||
      (!needed && !collator.empty())) {
    ESP_LOGW(kTag, "db collator is mismatched");
    return false;
  }

  return true;
}

auto Database::Open(IFileGatherer& gatherer,
                    ITagParser& parser,
                    locale::ICollator& collator,
                    tasks::Worker& bg_worker)
    -> cpp::result<Database*, DatabaseError> {
  if (sIsDbOpen.exchange(true)) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  if (!leveldb::sBackgroundThread) {
    leveldb::sBackgroundThread = &bg_worker;
  }

  std::shared_ptr<tasks::Worker> worker(
      tasks::Worker::Start<tasks::Type::kDatabase>());
  return worker
      ->Dispatch<cpp::result<Database*, DatabaseError>>(
          [&]() -> cpp::result<Database*, DatabaseError> {
            leveldb::DB* db;
            std::unique_ptr<leveldb::Cache> cache{
                leveldb::NewLRUCache(24 * 1024)};

            leveldb::Options options;
            options.env = sEnv.env();
            options.write_buffer_size = 48 * 1024;
            options.max_file_size = 32;
            options.block_cache = cache.get();
            options.block_size = 512;

            auto status = leveldb::DB::Open(options, kDbPath, &db);
            if (!status.ok()) {
              ESP_LOGI(kTag, "opening db failed. recreating.");
              db = CreateNewDatabase(options, collator);
              if (db == nullptr) {
                return cpp::fail(FAILED_TO_OPEN);
              }
            }

            if (!CheckDatabase(*db, collator)) {
              ESP_LOGI(kTag, "db incompatible. recreating.");
              delete db;
              db = CreateNewDatabase(options, collator);
              if (db == nullptr) {
                return cpp::fail(FAILED_TO_OPEN);
              }
            }

            ESP_LOGI(kTag, "Database opened successfully");
            return new Database(db, cache.release(), gatherer, parser, collator,
                                worker);
          })
      .get();
}

auto Database::Destroy() -> void {
  leveldb::Options options;
  options.env = sEnv.env();
  leveldb::DestroyDB(kDbPath, options);
}

Database::Database(leveldb::DB* db,
                   leveldb::Cache* cache,
                   IFileGatherer& file_gatherer,
                   ITagParser& tag_parser,
                   locale::ICollator& collator,
                   std::shared_ptr<tasks::Worker> worker)
    : db_(db),
      cache_(cache),
      worker_task_(worker),
      file_gatherer_(file_gatherer),
      tag_parser_(tag_parser),
      collator_(collator) {}

Database::~Database() {
  // Delete db_ first so that any outstanding background work finishes before
  // the background task is killed.
  delete db_;
  delete cache_;

  sIsDbOpen.store(false);
}

auto Database::Put(const std::string& key, const std::string& val) -> void {
  db_->Put(leveldb::WriteOptions{}, kKeyCustom + key, val);
}

auto Database::Get(const std::string& key) -> std::optional<std::string> {
  std::string val;
  auto res = db_->Get(leveldb::ReadOptions{}, kKeyCustom + key, &val);
  if (!res.ok()) {
    return {};
  }
  return val;
}

auto Database::Update() -> std::future<void> {
  events::Ui().Dispatch(event::UpdateStarted{});
  return worker_task_->Dispatch<void>([&]() -> void {
    leveldb::ReadOptions read_options;
    read_options.fill_cache = false;

    std::pair<uint16_t, uint16_t> newest_track{0, 0};

    // Stage 1: verify all existing tracks are still valid.
    ESP_LOGI(kTag, "verifying existing tracks");
    {
      uint64_t num_processed = 0;
      std::unique_ptr<leveldb::Iterator> it{db_->NewIterator(read_options)};
      std::string prefix = EncodeDataPrefix();
      for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix);
           it->Next()) {
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
          continue;
        }

        if (track->is_tombstoned) {
          ESP_LOGW(kTag, "skipping tombstoned %lx", track->id);
          continue;
        }

        FRESULT res;
        FILINFO info;
        {
          auto lock = drivers::acquire_spi();
          res = f_stat(track->filepath.c_str(), &info);
        }

        std::pair<uint16_t, uint16_t> modified_at{0, 0};
        if (res == FR_OK) {
          modified_at = {info.fdate, info.ftime};
        }
        if (modified_at == track->modified_at) {
          newest_track = std::max(modified_at, newest_track);
          continue;
        } else {
          track->modified_at = modified_at;
        }

        std::shared_ptr<TrackTags> tags =
            tag_parser_.ReadAndParseTags(track->filepath);
        if (!tags || tags->encoding() == Container::kUnsupported) {
          // We couldn't read the tags for this track. Either they were
          // malformed, or perhaps the file is missing. Either way, tombstone
          // this record.
          ESP_LOGW(kTag, "entombing missing #%lx", track->id);
          dbRemoveIndexes(track);
          track->is_tombstoned = true;
          dbPutTrackData(*track);
          continue;
        }

        // At this point, we know that the track still exists in its original
        // location. All that's left to do is update any metadata about it.

        uint64_t new_hash = tags->Hash();
        if (new_hash != track->tags_hash) {
          // This track's tags have changed. Since the filepath is exactly the
          // same, we assume this is a legitimate correction. Update the
          // database.
          ESP_LOGI(kTag, "updating hash (%llx -> %llx)", track->tags_hash,
                   new_hash);
          dbRemoveIndexes(track);

          track->tags_hash = new_hash;
          dbIngestTagHashes(*tags, track->individual_tag_hashes);
          dbPutTrackData(*track);
          dbPutHash(new_hash, track->id);
        }
      }
    }

    ESP_LOGI(kTag, "newest unmodified was at %u,%u", newest_track.first,
             newest_track.second);

    // Stage 2: search for newly added files.
    ESP_LOGI(kTag, "scanning for new tracks");
    uint64_t num_processed = 0;
    file_gatherer_.FindFiles("", [&](const std::pmr::string& path,
                                     const FILINFO& info) {
      num_processed++;
      events::Ui().Dispatch(event::UpdateProgress{
          .stage = event::UpdateProgress::Stage::kScanningForNewTracks,
          .val = num_processed,
      });

      std::pair<uint16_t, uint16_t> modified{info.fdate, info.ftime};
      if (modified < newest_track) {
        return;
      }

      std::shared_ptr<TrackTags> tags = tag_parser_.ReadAndParseTags(path);
      if (!tags || tags->encoding() == Container::kUnsupported) {
        // No parseable tags; skip this fiile.
        return;
      }

      // Check for any existing record with the same hash.
      uint64_t hash = tags->Hash();
      std::string key = EncodeHashKey(hash);
      std::optional<TrackId> existing_hash;
      std::string raw_entry;
      if (db_->Get(leveldb::ReadOptions(), key, &raw_entry).ok()) {
        existing_hash = ParseHashValue(raw_entry);
      }

      if (!existing_hash) {
        // We've never met this track before! Or we have, but the entry is
        // malformed. Either way, record this as a new track.
        TrackId id = dbMintNewTrackId();
        ESP_LOGI(kTag, "recording new 0x%lx", id);

        auto data = std::make_shared<TrackData>();
        data->id = id;
        data->filepath = path;
        data->tags_hash = hash;
        data->modified_at = modified;
        dbIngestTagHashes(*tags, data->individual_tag_hashes);

        dbPutTrackData(*data);
        dbPutHash(hash, id);
        auto t = std::make_shared<Track>(data, tags);
        dbCreateIndexesForTrack(*t);
        return;
      }

      std::shared_ptr<TrackData> existing_data = dbGetTrackData(*existing_hash);
      if (!existing_data) {
        // We found a hash that matches, but there's no data record? Weird.
        auto new_data = std::make_shared<TrackData>();
        new_data->id = dbMintNewTrackId();
        new_data->filepath = path;
        new_data->tags_hash = hash;
        new_data->modified_at = modified;
        dbIngestTagHashes(*tags, new_data->individual_tag_hashes);
        dbPutTrackData(*new_data);
        auto t = std::make_shared<Track>(new_data, tags);
        dbCreateIndexesForTrack(*t);
        return;
      }

      if (existing_data->is_tombstoned) {
        ESP_LOGI(kTag, "exhuming track %lu", existing_data->id);
        existing_data->is_tombstoned = false;
        existing_data->modified_at = modified;
        dbPutTrackData(*existing_data);
        auto t = std::make_shared<Track>(existing_data, tags);
        dbCreateIndexesForTrack(*t);
      } else if (existing_data->filepath != path) {
        ESP_LOGW(kTag, "tag hash collision for %s and %s",
                 existing_data->filepath.c_str(), path.c_str());
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
          return track_data->filepath;
        }
        return {};
      });
}

auto Database::GetTrack(TrackId id) -> std::future<std::shared_ptr<Track>> {
  return worker_task_->Dispatch<std::shared_ptr<Track>>(
      [=, this]() -> std::shared_ptr<Track> {
        std::shared_ptr<TrackData> data = dbGetTrackData(id);
        if (!data || data->is_tombstoned) {
          return {};
        }
        std::shared_ptr<TrackTags> tags =
            tag_parser_.ReadAndParseTags(data->filepath);
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
          std::string key = EncodeDataKey(id);
          it->Seek(key);
          if (!it->Valid() || it->key() != key) {
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

auto Database::GetTracksByIndex(IndexId index, std::size_t page_size)
    -> std::future<Result<IndexRecord>*> {
  return worker_task_->Dispatch<Result<IndexRecord>*>(
      [=, this]() -> Result<IndexRecord>* {
        IndexKey::Header header{
            .id = index,
            .depth = 0,
            .components_hash = 0,
        };
        std::string prefix = EncodeIndexPrefix(header);
        Continuation c{.prefix = {prefix.data(), prefix.size()},
                       .start_key = {prefix.data(), prefix.size()},
                       .forward = true,
                       .was_prev_forward = true,
                       .page_size = page_size};
        return dbGetPage<IndexRecord>(c);
      });
}

auto Database::GetTracks(std::size_t page_size) -> std::future<Result<Track>*> {
  return worker_task_->Dispatch<Result<Track>*>([=, this]() -> Result<Track>* {
    std::string prefix = EncodeDataPrefix();
    Continuation c{.prefix = {prefix.data(), prefix.size()},
                   .start_key = {prefix.data(), prefix.size()},
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
  auto status = db_->Get(leveldb::ReadOptions(), kKeyTrackId, &val);
  if (status.ok()) {
    next_id = BytesToTrackId(val).value_or(next_id);
  } else if (!status.IsNotFound()) {
    // TODO(jacqueline): Handle this more.
    ESP_LOGE(kTag, "failed to get next track id");
  }

  if (!db_->Put(leveldb::WriteOptions(), kKeyTrackId,
                TrackIdToBytes(next_id + 1))
           .ok()) {
    ESP_LOGE(kTag, "failed to write next track id");
  }

  return next_id;
}

auto Database::dbEntomb(TrackId id, uint64_t hash) -> void {
  std::string key = EncodeHashKey(hash);
  std::string val = EncodeHashValue(id);
  if (!db_->Put(leveldb::WriteOptions(), key, val).ok()) {
    ESP_LOGE(kTag, "failed to entomb #%llx (id #%lx)", hash, id);
  }
}

auto Database::dbPutTrackData(const TrackData& s) -> void {
  std::string key = EncodeDataKey(s.id);
  std::string val = EncodeDataValue(s);
  if (!db_->Put(leveldb::WriteOptions(), key, val).ok()) {
    ESP_LOGE(kTag, "failed to write data for #%lx", s.id);
  }
}

auto Database::dbGetTrackData(TrackId id) -> std::shared_ptr<TrackData> {
  std::string key = EncodeDataKey(id);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for #%lx", id);
    return {};
  }
  return ParseDataValue(raw_val);
}

auto Database::dbPutHash(const uint64_t& hash, TrackId i) -> void {
  std::string key = EncodeHashKey(hash);
  std::string val = EncodeHashValue(i);
  if (!db_->Put(leveldb::WriteOptions(), key, val).ok()) {
    ESP_LOGE(kTag, "failed to write hash for #%lx", i);
  }
}

auto Database::dbGetHash(const uint64_t& hash) -> std::optional<TrackId> {
  std::string key = EncodeHashKey(hash);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for hash #%llx", hash);
    return {};
  }
  return ParseHashValue(raw_val);
}

auto Database::dbCreateIndexesForTrack(const Track& track) -> void {
  for (const IndexInfo& index : GetIndexes()) {
    leveldb::WriteBatch writes;
    auto entries = Index(collator_, index, track);
    for (const auto& it : entries) {
      writes.Put(EncodeIndexKey(it.first),
                 {it.second.data(), it.second.size()});
    }
    db_->Write(leveldb::WriteOptions(), &writes);
  }
}

auto Database::dbRemoveIndexes(std::shared_ptr<TrackData> data) -> void {
  auto tags = dbRecoverTagsFromHashes(data->individual_tag_hashes);
  if (!tags) {
    return;
  }
  Track track{data, tags};
  for (const IndexInfo& index : GetIndexes()) {
    auto entries = Index(collator_, index, track);
    for (auto it = entries.rbegin(); it != entries.rend(); it++) {
      auto key = EncodeIndexKey(it->first);
      auto status = db_->Delete(leveldb::WriteOptions{}, key);
      if (!status.ok()) {
        return;
      }

      std::unique_ptr<leveldb::Iterator> cursor{db_->NewIterator({})};
      cursor->Seek(key);
      cursor->Prev();

      auto prev_key = ParseIndexKey(cursor->key());
      if (prev_key && prev_key->header == it->first.header) {
        break;
      }

      cursor->Next();
      auto next_key = ParseIndexKey(cursor->key());
      if (next_key && next_key->header == it->first.header) {
        break;
      }
    }
  }
}

auto Database::dbIngestTagHashes(const TrackTags& tags,
                                 std::pmr::unordered_map<Tag, uint64_t>& out)
    -> void {
  leveldb::WriteBatch batch{};
  for (auto& entry : tags.tags()) {
    auto hash =
        komihash_stream_oneshot(entry.second.data(), entry.second.size(), 0);
    batch.Put(EncodeTagHashKey(hash), entry.second.c_str());
    out[entry.first] = hash;
  }
  db_->Write(leveldb::WriteOptions{}, &batch);
}

auto Database::dbRecoverTagsFromHashes(
    const std::pmr::unordered_map<Tag, uint64_t>& hashes)
    -> std::shared_ptr<TrackTags> {
  auto out = std::make_shared<TrackTags>();
  for (const auto& entry : hashes) {
    std::string value;
    auto res = db_->Get(leveldb::ReadOptions{}, EncodeTagHashKey(entry.second),
                        &value);
    if (!res.ok()) {
      ESP_LOGI(kTag, "failed to retrieve tag!");
      continue;
    }
    out->set(entry.first, {value.data(), value.size()});
  }
  return out;
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
    // We were going forwards, and now we want the previous page. Set the
    // search key to the first result we saw, and mark that it's off by one.
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

auto Database::dbCount(const Continuation& c) -> size_t {
  std::unique_ptr<leveldb::Iterator> it{
      db_->NewIterator(leveldb::ReadOptions{})};
  size_t count = 0;
  for (it->Seek({c.start_key.data(), c.start_key.size()});
       it->Valid() && it->key().starts_with({c.prefix.data(), c.prefix.size()});
       it->Next()) {
    count++;
  }
  return count;
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
  if (!data || data->is_tombstoned) {
    return {};
  }
  std::shared_ptr<TrackTags> tags =
      tag_parser_.ReadAndParseTags(data->filepath);
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
  std::string new_prefix = EncodeIndexPrefix(ExpandHeader());
  return Continuation{
      .prefix = {new_prefix.data(), new_prefix.size()},
      .start_key = {new_prefix.data(), new_prefix.size()},
      .forward = true,
      .was_prev_forward = true,
      .page_size = page_size,
  };
}

auto IndexRecord::ExpandHeader() const -> IndexKey::Header {
  return ::database::ExpandHeader(key_.header, key_.item);
}

Iterator::Iterator(std::weak_ptr<Database> db, const IndexInfo& idx)
    : db_(db), pos_mutex_(), current_pos_(), prev_pos_() {
  std::string prefix = EncodeIndexPrefix(
      IndexKey::Header{.id = idx.id, .depth = 0, .components_hash = 0});
  current_pos_ = Continuation{.prefix = {prefix.data(), prefix.size()},
                              .start_key = {prefix.data(), prefix.size()},
                              .forward = true,
                              .was_prev_forward = true,
                              .page_size = 1};
}

auto Iterator::Parse(std::weak_ptr<Database> db, const cppbor::Array& encoded)
    -> std::optional<Iterator> {
  // Ensure the input looks reasonable.
  if (encoded.size() != 3) {
    return {};
  }

  if (encoded[0]->type() != cppbor::TSTR) {
    return {};
  }
  const std::string& prefix = encoded[0]->asTstr()->value();

  std::optional<Continuation> current_pos{};
  if (encoded[1]->type() == cppbor::TSTR) {
    const std::string& key = encoded[1]->asTstr()->value();
    current_pos = Continuation{
        .prefix = {prefix.data(), prefix.size()},
        .start_key = {key.data(), key.size()},
        .forward = true,
        .was_prev_forward = true,
        .page_size = 1,
    };
  }

  std::optional<Continuation> prev_pos{};
  if (encoded[2]->type() == cppbor::TSTR) {
    const std::string& key = encoded[2]->asTstr()->value();
    current_pos = Continuation{
        .prefix = {prefix.data(), prefix.size()},
        .start_key = {key.data(), key.size()},
        .forward = false,
        .was_prev_forward = false,
        .page_size = 1,
    };
  }

  return Iterator{db, std::move(current_pos), std::move(prev_pos)};
}

Iterator::Iterator(std::weak_ptr<Database> db, const Continuation& c)
    : db_(db), pos_mutex_(), current_pos_(c), prev_pos_() {}

Iterator::Iterator(const Iterator& other)
    : db_(other.db_),
      pos_mutex_(),
      current_pos_(other.current_pos_),
      prev_pos_(other.prev_pos_) {}

Iterator::Iterator(std::weak_ptr<Database> db,
                   std::optional<Continuation>&& cur,
                   std::optional<Continuation>&& prev)
    : db_(db), current_pos_(cur), prev_pos_(prev) {}

Iterator& Iterator::operator=(const Iterator& other) {
  current_pos_ = other.current_pos_;
  prev_pos_ = other.prev_pos_;
  return *this;
}

auto Iterator::Next(Callback cb) -> void {
  auto db = db_.lock();
  if (!db) {
    InvokeNull(cb);
    return;
  }
  db->worker_task_->Dispatch<void>([=]() {
    std::lock_guard lock{pos_mutex_};
    if (!current_pos_) {
      InvokeNull(cb);
      return;
    }
    std::unique_ptr<Result<IndexRecord>> res{
        db->dbGetPage<IndexRecord>(*current_pos_)};
    prev_pos_ = current_pos_;
    current_pos_ = res->next_page();
    if (!res || res->values().empty() || !res->values()[0]) {
      ESP_LOGI(kTag, "dropping empty result");
      InvokeNull(cb);
      return;
    }
    std::invoke(cb, *res->values()[0]);
  });
}

auto Iterator::NextSync() -> std::optional<IndexRecord> {
  auto db = db_.lock();
  if (!db) {
    return {};
  }
  std::lock_guard lock{pos_mutex_};
  if (!current_pos_) {
    return {};
  }
  std::unique_ptr<Result<IndexRecord>> res{
      db->dbGetPage<IndexRecord>(*current_pos_)};
  prev_pos_ = current_pos_;
  current_pos_ = res->next_page();
  if (!res || res->values().empty() || !res->values()[0]) {
    ESP_LOGI(kTag, "dropping empty result");
    return {};
  }
  return *res->values()[0];
}

auto Iterator::PeekSync() -> std::optional<IndexRecord> {
  auto db = db_.lock();
  if (!db) {
    return {};
  }
  auto pos = current_pos_;
  if (!pos) {
    return {};
  }
  std::unique_ptr<Result<IndexRecord>> res{db->dbGetPage<IndexRecord>(*pos)};
  if (!res || res->values().empty() || !res->values()[0]) {
    return {};
  }
  return *res->values()[0];
}

auto Iterator::Prev(Callback cb) -> void {
  auto db = db_.lock();
  if (!db) {
    InvokeNull(cb);
    return;
  }
  db->worker_task_->Dispatch<void>([=]() {
    std::lock_guard lock{pos_mutex_};
    if (!prev_pos_) {
      InvokeNull(cb);
      return;
    }
    std::unique_ptr<Result<IndexRecord>> res{
        db->dbGetPage<IndexRecord>(*current_pos_)};
    current_pos_ = prev_pos_;
    prev_pos_ = res->prev_page();
    std::invoke(cb, *res->values()[0]);
  });
}

auto Iterator::Size() const -> size_t {
  auto db = db_.lock();
  if (!db) {
    return {};
  }
  std::optional<Continuation> pos = current_pos_;
  if (!pos) {
    return 0;
  }
  return db->dbCount(*pos);
}

auto Iterator::InvokeNull(Callback cb) -> void {
  std::invoke(cb, std::optional<IndexRecord>{});
}

auto Iterator::cbor() const -> cppbor::Array&& {
  cppbor::Array res;

  std::pmr::string prefix;
  if (current_pos_) {
    prefix = current_pos_->prefix;
  } else if (prev_pos_) {
    prefix = prev_pos_->prefix;
  } else {
    ESP_LOGW(kTag, "iterator has no prefix");
    return std::move(res);
  }

  if (current_pos_) {
    res.add(cppbor::Tstr(current_pos_->start_key));
  } else {
    res.add(cppbor::Null());
  }

  if (prev_pos_) {
    res.add(cppbor::Tstr(prev_pos_->start_key));
  } else {
    res.add(cppbor::Null());
  }

  return std::move(res);
}

auto TrackIterator::Parse(std::weak_ptr<Database> db,
                          const cppbor::Array& encoded)
    -> std::optional<TrackIterator> {
  TrackIterator ret{db};

  for (const auto& item : encoded) {
    if (item->type() == cppbor::ARRAY) {
      auto it = Iterator::Parse(db, *item->asArray());
      if (it) {
        ret.levels_.push_back(std::move(*it));
      } else {
        return {};
      }
    }
  }

  return ret;
}

TrackIterator::TrackIterator(const Iterator& it) : db_(it.db_), levels_() {
  if (it.current_pos_) {
    levels_.push_back(it);
  }
  NextLeaf();
}

TrackIterator::TrackIterator(const TrackIterator& other)
    : db_(other.db_), levels_(other.levels_) {}

TrackIterator::TrackIterator(std::weak_ptr<Database> db) : db_(db), levels_() {}

TrackIterator& TrackIterator::operator=(TrackIterator&& other) {
  levels_ = std::move(other.levels_);
  return *this;
}

auto TrackIterator::Next() -> std::optional<TrackId> {
  std::optional<TrackId> next{};
  while (!next && !levels_.empty()) {
    auto next_record = levels_.back().NextSync();
    if (!next_record) {
      levels_.pop_back();
      NextLeaf();
      continue;
    }
    // May still be nullopt_t; hence the loop.
    next = next_record->track();
  }
  return next;
}

auto TrackIterator::Size() const -> size_t {
  size_t size = 0;
  TrackIterator copy{*this};
  while (!copy.levels_.empty()) {
    size += copy.levels_.back().Size();
    copy.levels_.pop_back();
    copy.NextLeaf();
  }
  return size;
}

auto TrackIterator::NextLeaf() -> void {
  while (!levels_.empty()) {
    ESP_LOGI(kTag, "check next candidate");
    Iterator& candidate = levels_.back();
    auto next = candidate.PeekSync();
    if (!next) {
      ESP_LOGI(kTag, "candidate is empty");
      levels_.pop_back();
      continue;
    }
    if (!next->track()) {
      ESP_LOGI(kTag, "candidate is a branch");
      candidate.NextSync();
      levels_.push_back(Iterator{db_, next->Expand(1).value()});
      continue;
    }
    ESP_LOGI(kTag, "candidate is a leaf");
    break;
  }
}

auto TrackIterator::cbor() const -> cppbor::Array&& {
  cppbor::Array res;
  for (const auto& i : levels_) {
    res.add(i.cbor());
  }
  return std::move(res);
}

}  // namespace database
