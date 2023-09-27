/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "file_gatherer.hpp"
#include "index.hpp"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "memory_resource.hpp"
#include "records.hpp"
#include "result.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "track.hpp"

namespace database {

template <typename T>
struct Continuation {
  std::pmr::string prefix;
  std::pmr::string start_key;
  bool forward;
  bool was_prev_forward;
  size_t page_size;
};

/*
 * Wrapper for a set of results from the database. Owns the list of results, as
 * well as a continuation token that can be used to continue fetching more
 * results if they were paginated.
 */
template <typename T>
class Result {
 public:
  auto values() const -> const std::vector<std::shared_ptr<T>>& {
    return values_;
  }

  auto next_page() -> std::optional<Continuation<T>>& { return next_page_; }
  auto prev_page() -> std::optional<Continuation<T>>& { return prev_page_; }

  Result(const std::vector<std::shared_ptr<T>>&& values,
         std::optional<Continuation<T>> next,
         std::optional<Continuation<T>> prev)
      : values_(values), next_page_(next), prev_page_(prev) {}

  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

 private:
  std::vector<std::shared_ptr<T>> values_;
  std::optional<Continuation<T>> next_page_;
  std::optional<Continuation<T>> prev_page_;
};

class IndexRecord {
 public:
  explicit IndexRecord(const IndexKey&,
                       std::optional<std::pmr::string>,
                       std::optional<TrackId>);

  auto text() const -> std::optional<std::pmr::string>;
  auto track() const -> std::optional<TrackId>;

  auto Expand(std::size_t) const -> std::optional<Continuation<IndexRecord>>;

 private:
  IndexKey key_;
  std::optional<std::pmr::string> override_text_;
  std::optional<TrackId> track_;
};

class Database {
 public:
  enum DatabaseError {
    ALREADY_OPEN,
    FAILED_TO_OPEN,
  };
  static auto Open(IFileGatherer& file_gatherer, ITagParser& tag_parser)
      -> cpp::result<Database*, DatabaseError>;
  static auto Open() -> cpp::result<Database*, DatabaseError>;

  static auto Destroy() -> void;

  ~Database();

  auto Update() -> std::future<void>;

  auto GetTrackPath(TrackId id) -> std::future<std::optional<std::pmr::string>>;

  auto GetTrack(TrackId id) -> std::future<std::shared_ptr<Track>>;

  /*
   * Fetches data for multiple tracks more efficiently than multiple calls to
   * GetTrack.
   */
  auto GetBulkTracks(std::vector<TrackId> id)
      -> std::future<std::vector<std::shared_ptr<Track>>>;

  auto GetIndexes() -> std::vector<IndexInfo>;
  auto GetTracksByIndex(const IndexInfo& index, std::size_t page_size)
      -> std::future<Result<IndexRecord>*>;
  auto GetTracks(std::size_t page_size) -> std::future<Result<Track>*>;
  auto GetDump(std::size_t page_size) -> std::future<Result<std::pmr::string>*>;

  template <typename T>
  auto GetPage(Continuation<T>* c) -> std::future<Result<T>*>;

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

 private:
  // Owned. Dumb pointers because destruction needs to be done in an explicit
  // order.
  leveldb::DB* db_;
  leveldb::Cache* cache_;

  std::shared_ptr<tasks::Worker> worker_task_;

  // Not owned.
  IFileGatherer& file_gatherer_;
  ITagParser& tag_parser_;

  Database(leveldb::DB* db,
           leveldb::Cache* cache,
           IFileGatherer& file_gatherer,
           ITagParser& tag_parser,
           std::shared_ptr<tasks::Worker> worker);

  auto dbMintNewTrackId() -> TrackId;
  auto dbEntomb(TrackId track, uint64_t hash) -> void;

  auto dbPutTrackData(const TrackData& s) -> void;
  auto dbGetTrackData(TrackId id) -> std::shared_ptr<TrackData>;
  auto dbPutHash(const uint64_t& hash, TrackId i) -> void;
  auto dbGetHash(const uint64_t& hash) -> std::optional<TrackId>;
  auto dbCreateIndexesForTrack(const Track& track) -> void;

  template <typename T>
  auto dbGetPage(const Continuation<T>& c) -> Result<T>*;

  template <typename T>
  auto ParseRecord(const leveldb::Slice& key, const leveldb::Slice& val)
      -> std::shared_ptr<T>;
};

template <>
auto Database::ParseRecord<IndexRecord>(const leveldb::Slice& key,
                                        const leveldb::Slice& val)
    -> std::shared_ptr<IndexRecord>;
template <>
auto Database::ParseRecord<Track>(const leveldb::Slice& key,
                                  const leveldb::Slice& val)
    -> std::shared_ptr<Track>;
template <>
auto Database::ParseRecord<std::pmr::string>(const leveldb::Slice& key,
                                             const leveldb::Slice& val)
    -> std::shared_ptr<std::pmr::string>;

}  // namespace database
