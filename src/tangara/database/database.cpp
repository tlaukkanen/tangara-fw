/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/database.hpp"

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

#include "cppbor.h"
#include "cppbor_parse.h"
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

#include "collation.hpp"
#include "database/db_events.hpp"
#include "database/env_esp.hpp"
#include "database/index.hpp"
#include "database/records.hpp"
#include "database/tag_parser.hpp"
#include "database/track.hpp"
#include "database/track_finder.hpp"
#include "events/event_queue.hpp"
#include "memory_resource.hpp"
#include "result.hpp"
#include "tasks.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
[[maybe_unused]] static const char* kTag = "DB";

static const char kDbPath[] = "/.tangara-db";
static const char kMusicPath[] = "Music";

static const char kKeyDbVersion[] = "schema_version";
static const char kKeyCustom[] = "U\0";
static const char kKeyCollator[] = "collator";

static constexpr size_t kMaxParallelism = 2;

static std::atomic<bool> sIsDbOpen(false);

using std::placeholders::_1;
using std::placeholders::_2;

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

auto Database::Open(ITagParser& parser,
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
            return new Database(db, cache.release(), bg_worker, parser,
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
                   tasks::WorkerPool& pool,
                   ITagParser& tag_parser,
                   locale::ICollator& collator)
    : db_(db),
      cache_(cache),
      track_finder_(
          pool,
          kMaxParallelism,
          std::bind(&Database::processCandidateCallback, this, _1, _2),
          std::bind(&Database::indexingCompleteCallback, this)),
      tag_parser_(tag_parser),
      collator_(collator),
      is_updating_(false) {
  dbCalculateNextTrackId();
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
  auto track_data = dbGetTrackData(leveldb::ReadOptions(), id);
  if (!track_data) {
    return {};
  }
  return std::string{track_data->filepath.data(), track_data->filepath.size()};
}

auto Database::getTrack(TrackId id) -> std::shared_ptr<Track> {
  std::shared_ptr<TrackData> data = dbGetTrackData(leveldb::ReadOptions(), id);
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

Database::UpdateTracker::UpdateTracker()
    : num_old_tracks_(0),
      num_new_tracks_(0),
      start_time_(esp_timer_get_time()) {
  events::Ui().Dispatch(event::UpdateStarted{});
  events::System().Dispatch(event::UpdateStarted{});
}

Database::UpdateTracker::~UpdateTracker() {
  uint64_t end_time = esp_timer_get_time();

  uint64_t time_per_old = 0;
  if (num_old_tracks_) {
    time_per_old = (verification_finish_time_ - start_time_) / num_old_tracks_;
  }
  uint64_t time_per_new = 0;
  if (num_new_tracks_) {
    time_per_new = (end_time - verification_finish_time_) / num_new_tracks_;
  }

  ESP_LOGI(
      kTag,
      "processed %lu old tracks and %lu new tracks in %llu seconds (%llums "
      "per old, %llums per new)",
      num_old_tracks_, num_new_tracks_, (end_time - start_time_) / 1000000,
      time_per_old / 1000, time_per_new / 1000);

  events::Ui().Dispatch(event::UpdateFinished{});
  events::System().Dispatch(event::UpdateFinished{});
}

auto Database::UpdateTracker::onTrackVerified() -> void {
  events::Ui().Dispatch(event::UpdateProgress{
      .stage = event::UpdateProgress::Stage::kVerifyingExistingTracks,
      .val = ++num_old_tracks_,
  });
}

auto Database::UpdateTracker::onVerificationFinished() -> void {
  verification_finish_time_ = esp_timer_get_time();
}

auto Database::UpdateTracker::onTrackAdded() -> void {
  num_new_tracks_++;
}

auto Database::updateIndexes() -> void {
  if (is_updating_.exchange(true)) {
    return;
  }
  update_tracker_ = std::make_unique<UpdateTracker>();

  leveldb::ReadOptions read_options;
  read_options.fill_cache = false;
  read_options.verify_checksums = true;

  // Stage 1: verify all existing tracks are still valid.
  ESP_LOGI(kTag, "verifying existing tracks");
  {
    std::unique_ptr<leveldb::Iterator> it{db_->NewIterator(read_options)};
    std::string prefix = EncodeDataPrefix();
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix);
         it->Next()) {
      update_tracker_->onTrackVerified();

      std::shared_ptr<TrackData> track = ParseDataValue(it->value());
      if (!track) {
        // The value was malformed. Drop this record.
        ESP_LOGW(kTag, "dropping malformed metadata");
        db_->Delete(leveldb::WriteOptions(), it->key());
        continue;
      }

      if (track->is_tombstoned) {
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
        ESP_LOGI(kTag, "entombing missing #%lx", track->id);

        // Remove the indexes first, so that interrupted operations don't leave
        // dangling index records.
        dbRemoveIndexes(track);

        // Do the rest of the tombstoning as one atomic write.
        leveldb::WriteBatch batch;
        track->is_tombstoned = true;
        batch.Put(EncodeDataKey(track->id), EncodeDataValue(*track));
        batch.Delete(EncodePathKey(track->filepath));

        db_->Write(leveldb::WriteOptions(), &batch);
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

        // Again, we remove the old index records first so has to avoid
        // dangling references.
        dbRemoveIndexes(track);

        // Atomically correct the hash + create the new index records.
        leveldb::WriteBatch batch;
        track->tags_hash = new_hash;
        dbIngestTagHashes(*tags, track->individual_tag_hashes, batch);

        dbCreateIndexesForTrack(*track, *tags, batch);
        batch.Put(EncodeDataKey(track->id), EncodeDataValue(*track));
        batch.Put(EncodeHashKey(new_hash), EncodeHashValue(track->id));
        db_->Write(leveldb::WriteOptions(), &batch);
      }
    }
  }

  update_tracker_->onVerificationFinished();

  // Stage 2: search for newly added files.
  std::string root;
  FF_DIR dir;
  if (f_opendir(&dir, kMusicPath) == FR_OK) {
    f_closedir(&dir);
    root = kMusicPath;
  }
  ESP_LOGI(kTag, "scanning for new tracks in '%s'", root.c_str());
  track_finder_.launch(root);
};

auto Database::processCandidateCallback(FILINFO& info, std::string_view path)
    -> void {
  leveldb::ReadOptions read_options;
  read_options.fill_cache = true;
  read_options.verify_checksums = false;

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

  // Check for any existing track with the same hash.
  uint64_t hash = tags->Hash();
  std::optional<TrackId> existing_id;
  std::string raw_entry;
  if (db_->Get(read_options, EncodeHashKey(hash), &raw_entry).ok()) {
    existing_id = ParseHashValue(raw_entry);
  }

  std::shared_ptr<TrackData> data;
  if (existing_id) {
    // Do we have any existing data for this track? This could be the case if
    // this is a tombstoned entry. In such as case, we want to reuse the
    // previous TrackData so that any extra metadata is preserved.
    data = dbGetTrackData(read_options, *existing_id);
    if (!data) {
      data = std::make_shared<TrackData>();
      data->id = *existing_id;
    } else if (data->filepath != path && !data->is_tombstoned) {
      ESP_LOGW(kTag, "hash collision: %s, %s, %s",
               tags->title().value_or("no title").c_str(),
               tags->artist().value_or("no artist").c_str(),
               tags->album().value_or("no album").c_str());
      // Don't commit anything if there's a hash collision, since we're
      // likely to make a big mess.
      return;
    }
  } else {
    update_tracker_->onTrackAdded();
    data = std::make_shared<TrackData>();
    data->id = dbMintNewTrackId();
  }

  // Make sure the file-based metadata on the TrackData is up to date.
  data->filepath = path;
  data->tags_hash = hash;
  data->modified_at = {info.fdate, info.ftime};
  data->is_tombstoned = false;

  // Apply all the actual database changes as one atomic batch. This makes
  // the whole 'new track' operation atomic, and also reduces the amount of
  // lock contention when adding many tracks at once.
  leveldb::WriteBatch batch;
  dbIngestTagHashes(*tags, data->individual_tag_hashes, batch);

  dbCreateIndexesForTrack(*data, *tags, batch);
  batch.Put(EncodeDataKey(data->id), EncodeDataValue(*data));
  batch.Put(EncodeHashKey(data->tags_hash), EncodeHashValue(data->id));
  batch.Put(EncodePathKey(path), TrackIdToBytes(data->id));

  db_->Write(leveldb::WriteOptions(), &batch);
}

auto Database::indexingCompleteCallback() -> void {
  update_tracker_.reset();
  is_updating_ = false;
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

auto Database::dbGetTrackData(leveldb::ReadOptions options, TrackId id)
    -> std::shared_ptr<TrackData> {
  std::string key = EncodeDataKey(id);
  std::string raw_val;
  if (!db_->Get(options, key, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for #%lx", id);
    return {};
  }
  return ParseDataValue(raw_val);
}

auto Database::dbCreateIndexesForTrack(const Track& track,
                                       leveldb::WriteBatch& batch) -> void {
  dbCreateIndexesForTrack(track.data(), track.tags(), batch);
}

auto Database::dbCreateIndexesForTrack(const TrackData& data,
                                       const TrackTags& tags,
                                       leveldb::WriteBatch& batch) -> void {
  for (const IndexInfo& index : getIndexes()) {
    auto entries = Index(collator_, index, data, tags);
    for (const auto& it : entries) {
      batch.Put(EncodeIndexKey(it.first), {it.second.data(), it.second.size()});
    }
  }
}

auto Database::dbRemoveIndexes(std::shared_ptr<TrackData> data) -> void {
  auto tags = dbRecoverTagsFromHashes(data->individual_tag_hashes);
  if (!tags) {
    return;
  }
  for (const IndexInfo& index : getIndexes()) {
    auto entries = Index(collator_, index, *data, *tags);
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
                                 std::pmr::unordered_map<Tag, uint64_t>& out,
                                 leveldb::WriteBatch& batch) -> void {
  for (const auto& tag : tags.allPresent()) {
    auto val = tags.get(tag);
    auto hash = tagHash(val);
    batch.Put(EncodeTagHashKey(hash), tagToString(val));
    out[tag] = hash;
  }
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
