/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/database.hpp"

#include <stdint.h>
#include <sys/_stdint.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

#include "collation.hpp"
#include "cppbor.h"
#include "cppbor_parse.h"
#include "database/index.hpp"
#include "debug.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "ff.h"
#include "freertos/projdefs.h"
#include "komihash.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/write_batch.h"

#include "database/db_events.hpp"
#include "database/env_esp.hpp"
#include "database/file_gatherer.hpp"
#include "database/records.hpp"
#include "database/tag_parser.hpp"
#include "database/track.hpp"
#include "drivers/spi.hpp"
#include "events/event_queue.hpp"
#include "memory_resource.hpp"
#include "result.hpp"
#include "tasks.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
[[maybe_unused]] static const char* kTag = "DB";

static const char kDbPath[] = "/.tangara-db";

static const char kKeyDbVersion[] = "schema_version";

static const char kKeyCustom[] = "U\0";
static const char kKeyCollator[] = "collator";

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
                    tasks::WorkerPool& bg_worker)
    -> cpp::result<Database*, DatabaseError> {
  if (sIsDbOpen.exchange(true)) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  if (!leveldb::sBackgroundThread) {
    leveldb::sBackgroundThread = &bg_worker;
  }

  return bg_worker
      .Dispatch<cpp::result<Database*, DatabaseError>>(
          [&]() -> cpp::result<Database*, DatabaseError> {
            leveldb::DB* db;
            std::unique_ptr<leveldb::Cache> cache{
                leveldb::NewLRUCache(256 * 1024)};

            leveldb::Options options;
            options.env = sEnv.env();
            // Match the write buffer size to the MMU page size in order to
            // make most efficient use of PSRAM mapping.
            options.write_buffer_size = CONFIG_MMU_PAGE_SIZE;
            options.block_cache = cache.get();

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
            return new Database(db, cache.release(), gatherer, parser,
                                collator);
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
                   locale::ICollator& collator)
    : db_(db),
      cache_(cache),
      file_gatherer_(file_gatherer),
      tag_parser_(tag_parser),
      collator_(collator),
      is_updating_(false) {
  dbCalculateNextTrackId();
  ESP_LOGI(kTag, "next track id is %lu", next_track_id_.load());
}

Database::~Database() {
  // Delete db_ first so that any outstanding background work finishes before
  // the background task is killed.
  delete db_;
  delete cache_;

  sIsDbOpen.store(false);
}

auto Database::schemaVersion() -> std::string {
  // If the database is open, then it must have the current schema.
  return std::to_string(kCurrentDbVersion);
}

auto Database::sizeOnDiskBytes() -> size_t {
  FF_DIR dir;
  FRESULT res = f_opendir(&dir, kDbPath);
  if (res != FR_OK) {
    return 0;
  }

  size_t total_size = 0;
  for (;;) {
    FILINFO info;
    res = f_readdir(&dir, &info);
    if (res != FR_OK || info.fname[0] == 0) {
      break;
    }
    total_size += info.fsize;
  }

  return total_size;
}

auto Database::put(const std::string& key, const std::string& val) -> void {
  if (val.empty()) {
    db_->Delete(leveldb::WriteOptions{}, kKeyCustom + key);
  } else {
    db_->Put(leveldb::WriteOptions{}, kKeyCustom + key, val);
  }
}

auto Database::get(const std::string& key) -> std::optional<std::string> {
  std::string val;
  auto res = db_->Get(leveldb::ReadOptions{}, kKeyCustom + key, &val);
  if (!res.ok() || val.empty()) {
    return {};
  }
  return val;
}

auto Database::getTrackPath(TrackId id) -> std::optional<std::string> {
  auto track_data = dbGetTrackData(id);
  if (!track_data) {
    return {};
  }
  return std::string{track_data->filepath.data(), track_data->filepath.size()};
}

auto Database::getTrack(TrackId id) -> std::shared_ptr<Track> {
  std::shared_ptr<TrackData> data = dbGetTrackData(id);
  if (!data || data->is_tombstoned) {
    return {};
  }
  std::shared_ptr<TrackTags> tags = tag_parser_.ReadAndParseTags(
      {data->filepath.data(), data->filepath.size()});
  if (!tags) {
    return {};
  }
  return std::make_shared<Track>(data, tags);
}

auto Database::getIndexes() -> std::vector<IndexInfo> {
  // TODO(jacqueline): This probably needs to be async? When we have runtime
  // configurable indexes, they will need to come from somewhere.
  return {
      kAllTracks,
      kAllAlbums,
      kAlbumsByArtist,
      kTracksByGenre,
  };
}

class UpdateNotifier {
 public:
  UpdateNotifier(std::atomic<bool>& is_updating) : is_updating_(is_updating) {
    events::Ui().Dispatch(event::UpdateStarted{});
    events::System().Dispatch(event::UpdateStarted{});
  }
  ~UpdateNotifier() {
    is_updating_ = false;
    events::Ui().Dispatch(event::UpdateFinished{});
    events::System().Dispatch(event::UpdateFinished{});
  }

 private:
  std::atomic<bool>& is_updating_;
};

auto Database::updateIndexes() -> void {
  if (is_updating_.exchange(true)) {
    return;
  }
  UpdateNotifier notifier{is_updating_};

  uint32_t num_old_tracks = 0;
  uint32_t num_new_tracks = 0;
  uint64_t start_time = esp_timer_get_time();

  leveldb::ReadOptions read_options;
  read_options.fill_cache = true;

  // Stage 1: verify all existing tracks are still valid.
  ESP_LOGI(kTag, "verifying existing tracks");
  {
    std::unique_ptr<leveldb::Iterator> it{db_->NewIterator(read_options)};
    std::string prefix = EncodeDataPrefix();
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix);
         it->Next()) {
      num_old_tracks++;
      events::Ui().Dispatch(event::UpdateProgress{
          .stage = event::UpdateProgress::Stage::kVerifyingExistingTracks,
          .val = num_old_tracks,
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

      FILINFO info;
      FRESULT res = f_stat(track->filepath.c_str(), &info);

      std::pair<uint16_t, uint16_t> modified_at{0, 0};
      if (res == FR_OK) {
        modified_at = {info.fdate, info.ftime};
      }
      if (modified_at == track->modified_at) {
        continue;
      } else {
        track->modified_at = modified_at;
      }

      std::shared_ptr<TrackTags> tags = tag_parser_.ReadAndParseTags(
          {track->filepath.data(), track->filepath.size()});
      if (!tags || tags->encoding() == Container::kUnsupported) {
        // We couldn't read the tags for this track. Either they were
        // malformed, or perhaps the file is missing. Either way, tombstone
        // this record.
        ESP_LOGW(kTag, "entombing missing #%lx", track->id);
        dbRemoveIndexes(track);
        track->is_tombstoned = true;
        dbPutTrackData(*track);
        db_->Delete(leveldb::WriteOptions{}, EncodePathKey(track->filepath));
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

  uint64_t verify_end_time = esp_timer_get_time();

  // Stage 2: search for newly added files.
  ESP_LOGI(kTag, "scanning for new tracks");
  uint64_t num_files = 0;
  file_gatherer_.FindFiles("", [&](std::string_view path, const FILINFO& info) {
    num_files++;
    events::Ui().Dispatch(event::UpdateProgress{
        .stage = event::UpdateProgress::Stage::kScanningForNewTracks,
        .val = num_files,
    });

    std::string unused;
    if (db_->Get(read_options, EncodePathKey(path), &unused).ok()) {
      // This file is already in the database; skip it.
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

    std::pair<uint16_t, uint16_t> modified{info.fdate, info.ftime};
    if (!existing_hash) {
      // We've never met this track before! Or we have, but the entry is
      // malformed. Either way, record this as a new track.
      TrackId id = dbMintNewTrackId();
      ESP_LOGD(kTag, "recording new 0x%lx", id);
      num_new_tracks++;

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
      db_->Put(leveldb::WriteOptions{}, EncodePathKey(path),
               TrackIdToBytes(id));
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
      db_->Put(leveldb::WriteOptions{}, EncodePathKey(path),
               TrackIdToBytes(new_data->id));
      return;
    }

    if (existing_data->is_tombstoned) {
      ESP_LOGI(kTag, "exhuming track %lu", existing_data->id);
      existing_data->is_tombstoned = false;
      existing_data->modified_at = modified;
      dbPutTrackData(*existing_data);
      auto t = std::make_shared<Track>(existing_data, tags);
      dbCreateIndexesForTrack(*t);
      db_->Put(leveldb::WriteOptions{}, EncodePathKey(path),
               TrackIdToBytes(existing_data->id));
    } else if (existing_data->filepath !=
               std::pmr::string{path.data(), path.size()}) {
      ESP_LOGW(kTag, "hash collision: %s, %s, %s",
               tags->title().value_or("no title").c_str(),
               tags->artist().value_or("no artist").c_str(),
               tags->album().value_or("no album").c_str());
    }
  });

  uint64_t end_time = esp_timer_get_time();

  uint64_t time_per_old = 0;
  if (num_old_tracks) {
    time_per_old = (verify_end_time - start_time) / num_old_tracks;
  }
  uint64_t time_per_new = 0;
  if (num_new_tracks) {
    time_per_new = (end_time - verify_end_time) / num_new_tracks;
  }

  ESP_LOGI(
      kTag,
      "processed %lu old tracks and %lu new tracks in %llu seconds (%llums "
      "per old, %llums per new)",
      num_old_tracks, num_new_tracks, (end_time - start_time) / 1000000,
      time_per_old / 1000, time_per_new / 1000);
}

auto Database::isUpdating() -> bool {
  return is_updating_;
}

auto Database::dbCalculateNextTrackId() -> void {
  std::unique_ptr<leveldb::Iterator> it{
      db_->NewIterator(leveldb::ReadOptions())};

  // Track data entries are of the format 'D/trackid', where track ids are
  // encoded as big-endian cbor types. They can therefore be compared through
  // byte ordering, which means we can determine what the next id should be by
  // looking at the larged track data record in the database.
  std::string prefix = EncodeDataPrefix();
  std::string prefixPlusOne = prefix;
  prefixPlusOne[prefixPlusOne.size() - 1]++;

  // Seek to just past the track data section.
  it->Seek(prefixPlusOne);
  if (!it->Valid()) {
    next_track_id_ = 1;
    return;
  }

  // Go back to the last track data record.
  it->Prev();
  if (!it->Valid() || !it->key().starts_with(prefix)) {
    next_track_id_ = 1;
    return;
  }

  // Parse the track id back out of the key.
  std::span<const char> key{it->key().data(), it->key().size()};
  auto id_part = key.subspan(prefix.size());
  if (id_part.empty()) {
    next_track_id_ = 1;
    return;
  }

  next_track_id_ = BytesToTrackId(id_part).value_or(0) + 1;
}

auto Database::dbMintNewTrackId() -> TrackId {
  return next_track_id_++;
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
  for (const IndexInfo& index : getIndexes()) {
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
  for (const IndexInfo& index : getIndexes()) {
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
  for (const auto& tag : tags.allPresent()) {
    auto val = tags.get(tag);
    auto hash = tagHash(val);
    batch.Put(EncodeTagHashKey(hash), tagToString(val));
    out[tag] = hash;
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

auto seekToOffset(leveldb::Iterator* it, int offset) {
  while (it->Valid() && offset != 0) {
    if (offset < 0) {
      it->Prev();
      offset++;
    } else {
      it->Next();
      offset--;
    }
  }
}

auto Database::getRecord(const SearchKey& c)
    -> std::optional<std::pair<std::pmr::string, Record>> {
  std::unique_ptr<leveldb::Iterator> it{
      db_->NewIterator(leveldb::ReadOptions{})};

  it->Seek(c.startKey());
  seekToOffset(it.get(), c.offset);
  if (!it->Valid() || !it->key().starts_with(std::string_view{c.prefix})) {
    return {};
  }

  std::optional<IndexKey> key = ParseIndexKey(it->key());
  if (!key) {
    ESP_LOGW(kTag, "parsing index key failed");
    return {};
  }

  return std::make_pair(std::pmr::string{it->key().data(), it->key().size(),
                                         &memory::kSpiRamResource},
                        Record{*key, it->value()});
}

auto Database::countRecords(const SearchKey& c) -> size_t {
  std::unique_ptr<leveldb::Iterator> it{
      db_->NewIterator(leveldb::ReadOptions{})};

  it->Seek(c.startKey());
  seekToOffset(it.get(), c.offset);
  if (!it->Valid() || !it->key().starts_with(std::string_view{c.prefix})) {
    return {};
  }

  size_t count = 0;
  while (it->Valid() && it->key().starts_with(std::string_view{c.prefix})) {
    it->Next();
    count++;
  }

  return count;
}

Handle::Handle(std::shared_ptr<Database>& db) : db_(db) {}

auto Handle::lock() -> std::shared_ptr<Database> {
  return db_;
}

auto SearchKey::startKey() const -> std::string_view {
  if (key) {
    return *key;
  }
  return prefix;
}

Record::Record(const IndexKey& key, const leveldb::Slice& t)
    : text_(t.data(), t.size(), &memory::kSpiRamResource) {
  if (key.track) {
    contents_ = *key.track;
  } else {
    contents_ = ExpandHeader(key.header, key.item);
  }
}

auto Record::text() const -> std::string_view {
  return text_;
}

auto Record::contents() const
    -> const std::variant<TrackId, IndexKey::Header>& {
  return contents_;
}

Iterator::Iterator(std::shared_ptr<Database> db, IndexId idx)
    : Iterator(db,
               IndexKey::Header{
                   .id = idx,
                   .depth = 0,
                   .components_hash = 0,
               }) {}

Iterator::Iterator(std::shared_ptr<Database> db, const IndexKey::Header& header)
    : db_(db), key_{}, current_() {
  std::string prefix = EncodeIndexPrefix(header);
  key_ = {
      .prefix = {prefix.data(), prefix.size(), &memory::kSpiRamResource},
      .key = {},
      .offset = -1,
  };
}

auto Iterator::value() const -> const std::optional<Record>& {
  return current_;
}

auto Iterator::next() -> void {
  SearchKey new_key = key_;
  if (new_key.offset == -1) {
    new_key.offset = 0;
  } else {
    new_key.offset = 1;
  }
  iterate(new_key);
}

auto Iterator::prev() -> void {
  SearchKey new_key = key_;
  new_key.offset = -1;
  iterate(new_key);
}

auto Iterator::iterate(const SearchKey& key) -> void {
  auto db = db_.lock();
  if (!db) {
    ESP_LOGW(kTag, "iterate with dead db");
    return;
  }
  auto res = db->getRecord(key);
  if (res) {
    key_ = {
        .prefix = key_.prefix,
        .key = res->first,
        .offset = 0,
    };
    current_ = res->second;
  } else {
    key_ = key;
    current_.reset();
  }
}

auto Iterator::count() const -> size_t {
  auto db = db_.lock();
  if (!db) {
    ESP_LOGW(kTag, "count with dead db");
    return 0;
  }
  return db->countRecords(key_);
}

TrackIterator::TrackIterator(const Iterator& it) : db_(it.db_), levels_() {
  levels_.push_back(it);
  next();
}

auto TrackIterator::next() -> void {
  while (!levels_.empty()) {
    levels_.back().next();

    auto& cur = levels_.back().value();
    if (!cur) {
      // The current top iterator is out of tracks. Pop it, and move the parent
      // to the next item.
      levels_.pop_back();
    } else if (std::holds_alternative<IndexKey::Header>(cur->contents())) {
      // This record is a branch. Push a new iterator.
      auto key = std::get<IndexKey::Header>(cur->contents());
      auto db = db_.lock();
      if (!db) {
        return;
      }
      levels_.emplace_back(db, key);
    } else if (std::holds_alternative<TrackId>(cur->contents())) {
      // New record is a leaf.
      break;
    }
  }
}

auto TrackIterator::value() const -> std::optional<TrackId> {
  if (levels_.empty()) {
    return {};
  }
  auto cur = levels_.back().value();
  if (!cur) {
    return {};
  }
  if (std::holds_alternative<TrackId>(cur->contents())) {
    return std::get<TrackId>(cur->contents());
  }
  return {};
}

auto TrackIterator::count() const -> size_t {
  size_t size = 0;
  TrackIterator copy{*this};
  while (!copy.levels_.empty()) {
    size += copy.levels_.back().count();
    copy.levels_.pop_back();
    copy.next();
  }
  return size;
}

}  // namespace database
