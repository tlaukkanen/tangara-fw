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
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "records.hpp"
#include "result.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "track.hpp"

namespace database {

template <typename T>
struct Continuation {
  std::shared_ptr<std::unique_ptr<leveldb::Iterator>> iterator;
  std::string prefix;
  std::string start_key;
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
  auto values() const -> const std::vector<T>& { return values_; }

  auto next_page() -> std::optional<Continuation<T>>& { return next_page_; }
  auto prev_page() -> std::optional<Continuation<T>>& { return prev_page_; }

  Result(const std::vector<T>&& values,
         std::optional<Continuation<T>> next,
         std::optional<Continuation<T>> prev)
      : values_(values), next_page_(next), prev_page_(prev) {}

  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

 private:
  std::vector<T> values_;
  std::optional<Continuation<T>> next_page_;
  std::optional<Continuation<T>> prev_page_;
};

class Database {
 public:
  enum DatabaseError {
    ALREADY_OPEN,
    FAILED_TO_OPEN,
  };
  static auto Open(IFileGatherer* file_gatherer, ITagParser* tag_parser)
      -> cpp::result<Database*, DatabaseError>;
  static auto Open() -> cpp::result<Database*, DatabaseError>;

  static auto Destroy() -> void;

  ~Database();

  auto Update() -> std::future<void>;

  auto GetTracks(std::size_t page_size) -> std::future<Result<Track>*>;
  auto GetDump(std::size_t page_size) -> std::future<Result<std::string>*>;

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
  IFileGatherer* file_gatherer_;
  ITagParser* tag_parser_;

  Database(leveldb::DB* db,
           leveldb::Cache* cache,
           IFileGatherer* file_gatherer,
           ITagParser* tag_parser,
           std::shared_ptr<tasks::Worker> worker);

  auto dbMintNewTrackId() -> TrackId;
  auto dbEntomb(TrackId track, uint64_t hash) -> void;

  auto dbPutTrackData(const TrackData& s) -> void;
  auto dbGetTrackData(TrackId id) -> std::optional<TrackData>;
  auto dbPutHash(const uint64_t& hash, TrackId i) -> void;
  auto dbGetHash(const uint64_t& hash) -> std::optional<TrackId>;
  auto dbPutTrack(TrackId id, const std::string& path, const uint64_t& hash)
      -> void;

  template <typename T>
  auto dbGetPage(const Continuation<T>& c) -> Result<T>*;

  template <typename T>
  auto ParseRecord(const leveldb::Slice& key, const leveldb::Slice& val)
      -> std::optional<T>;
};

template <>
auto Database::ParseRecord<Track>(const leveldb::Slice& key,
                                  const leveldb::Slice& val)
    -> std::optional<Track>;
template <>
auto Database::ParseRecord<std::string>(const leveldb::Slice& key,
                                        const leveldb::Slice& val)
    -> std::optional<std::string>;

}  // namespace database
