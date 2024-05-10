/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <sys/_stdint.h>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "collation.hpp"
#include "cppbor.h"
#include "database/file_gatherer.hpp"
#include "database/index.hpp"
#include "database/records.hpp"
#include "database/tag_parser.hpp"
#include "database/track.hpp"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "memory_resource.hpp"
#include "result.hpp"
#include "tasks.hpp"

namespace database {

const uint8_t kCurrentDbVersion = 6;

struct SearchKey;
class Record;
class Iterator;

/*
 * Handle to an open database. This can be used to store large amounts of
 * persistent data on the SD card, in a manner that can be retrieved later very
 * quickly.
 *
 * A database includes a number of 'indexes'. Each index is a sorted,
 * hierarchical view of all the playable tracks on the device.
 */
class Database {
 public:
  enum DatabaseError {
    ALREADY_OPEN,
    FAILED_TO_OPEN,
  };
  static auto Open(IFileGatherer& file_gatherer,
                   ITagParser& tag_parser,
                   locale::ICollator& collator,
                   tasks::WorkerPool& bg_worker)
      -> cpp::result<Database*, DatabaseError>;

  static auto Destroy() -> void;

  ~Database();

  auto schemaVersion() -> std::string;

  auto sizeOnDiskBytes() -> size_t;

  /* Adds an arbitrary record to the database. */
  auto put(const std::string& key, const std::string& val) -> void;

  /* Retrives a value previously stored with `put`. */
  auto get(const std::string& key) -> std::optional<std::string>;

  auto getTrackPath(TrackId id) -> std::optional<std::string>;
  auto getTrack(TrackId id) -> std::shared_ptr<Track>;

  auto getIndexes() -> std::vector<IndexInfo>;
  auto updateIndexes() -> void;
  auto isUpdating() -> bool;

  // Cannot be copied or moved.
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

 private:
  friend class Iterator;

  // Owned. Dumb pointers because destruction needs to be done in an explicit
  // order.
  leveldb::DB* db_;
  leveldb::Cache* cache_;

  // Not owned.
  IFileGatherer& file_gatherer_;
  ITagParser& tag_parser_;
  locale::ICollator& collator_;

  std::atomic<bool> is_updating_;

  Database(leveldb::DB* db,
           leveldb::Cache* cache,
           IFileGatherer& file_gatherer,
           ITagParser& tag_parser,
           locale::ICollator& collator);

  auto dbMintNewTrackId() -> TrackId;

  auto dbEntomb(TrackId track, uint64_t hash) -> void;
  auto dbPutTrackData(const TrackData& s) -> void;
  auto dbGetTrackData(TrackId id) -> std::shared_ptr<TrackData>;
  auto dbPutHash(const uint64_t& hash, TrackId i) -> void;
  auto dbGetHash(const uint64_t& hash) -> std::optional<TrackId>;

  auto dbCreateIndexesForTrack(const Track& track) -> void;
  auto dbRemoveIndexes(std::shared_ptr<TrackData>) -> void;

  auto dbIngestTagHashes(const TrackTags&,
                         std::pmr::unordered_map<Tag, uint64_t>&) -> void;
  auto dbRecoverTagsFromHashes(const std::pmr::unordered_map<Tag, uint64_t>&)
      -> std::shared_ptr<TrackTags>;

  auto getRecord(const SearchKey& c)
      -> std::optional<std::pair<std::pmr::string, Record>>;
  auto countRecords(const SearchKey& c) -> size_t;
};

/*
 * Container for the data needed to iterate through database records. This is a
 * lower-level type that the higher-level iterators are built from; most users
 * outside this namespace shouldn't need to work with continuations.
 */
struct SearchKey {
  std::pmr::string prefix;
  /* If not given, then iteration starts from `prefix`. */
  std::optional<std::pmr::string> key;
  int offset;

  auto startKey() const -> std::string_view;
};

/*
 * A record belonging to one of the database's indexes. This may either be a
 * leaf record, containing a track id, or a branch record, containing a new
 * Header to retrieve results at the next level of the index.
 */
class Record {
 public:
  Record(const IndexKey&, const leveldb::Slice&);

  Record(const Record&) = default;
  Record& operator=(const Record& other) = default;

  auto text() const -> std::string_view;
  auto contents() const -> const std::variant<TrackId, IndexKey::Header>&;

 private:
  std::pmr::string text_;
  std::variant<TrackId, IndexKey::Header> contents_;
};

/*
 * Utility for accessing a large set of database records, one record at a time.
 */
class Iterator {
 public:
  Iterator(std::shared_ptr<Database>, IndexId);
  Iterator(std::shared_ptr<Database>, const IndexKey::Header&);

  Iterator(const Iterator&) = default;
  Iterator& operator=(const Iterator& other) = default;

  auto value() const -> const std::optional<Record>&;
  std::optional<Record> operator*() const { return value(); }

  auto next() -> void;
  std::optional<Record> operator++() {
    next();
    return value();
  }
  std::optional<Record> operator++(int) {
    auto val = value();
    next();
    return val;
  }

  auto prev() -> void;
  std::optional<Record> operator--() {
    prev();
    return value();
  }
  std::optional<Record> operator--(int) {
    auto val = value();
    prev();
    return val;
  }

  auto count() const -> size_t;

 private:
  auto iterate(const SearchKey& key) -> void;

  friend class TrackIterator;

  std::weak_ptr<Database> db_;
  SearchKey key_;
  std::optional<Record> current_;
};

class TrackIterator {
 public:
  TrackIterator(const Iterator&);

  TrackIterator(const TrackIterator&) = default;
  TrackIterator& operator=(TrackIterator&& other) = default;

  auto value() const -> std::optional<TrackId>;
  std::optional<TrackId> operator*() const { return value(); }

  auto next() -> void;
  std::optional<TrackId> operator++() {
    next();
    return value();
  }
  std::optional<TrackId> operator++(int) {
    auto val = value();
    next();
    return val;
  }

  auto count() const -> size_t;

 private:
  TrackIterator(std::weak_ptr<Database>);
  auto next(bool advance) -> void;

  std::weak_ptr<Database> db_;
  std::vector<Iterator> levels_;
};

}  // namespace database
