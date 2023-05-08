#pragma once

#include <stdint.h>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "records.hpp"
#include "result.hpp"
#include "song.hpp"

namespace database {

typedef std::unique_ptr<leveldb::Iterator> Continuation;

/*
 * Wrapper for a set of results from the database. Owns the list of results, as
 * well as a continuation token that can be used to continue fetching more
 * results if they were paginated.
 */
template <typename T>
class Result {
 public:
  auto values() -> std::vector<T>* { return values_.release(); }
  auto continuation() -> Continuation { return std::move(c_); }
  auto HasMore() -> bool { return c_->Valid(); }

  Result(std::vector<T>* values, Continuation c)
      : values_(values), c_(std::move(c)) {}

  Result(std::unique_ptr<std::vector<T>> values, Continuation c)
      : values_(std::move(values)), c_(std::move(c)) {}

  Result(Result&& other)
      : values_(move(other.values_)), c_(std::move(other.c_)) {}

  Result operator=(Result&& other) {
    return Result(other.values(), std::move(other.continuation()));
  }

  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

 private:
  std::unique_ptr<std::vector<T>> values_;
  Continuation c_;
};

class Database {
 public:
  enum DatabaseError {
    ALREADY_OPEN,
    FAILED_TO_OPEN,
  };
  static auto Open() -> cpp::result<Database*, DatabaseError>;

  ~Database();

  auto Update() -> std::future<void>;
  auto Destroy() -> std::future<void>;

  auto GetSongs(std::size_t page_size) -> std::future<Result<Song>>;
  auto GetMoreSongs(std::size_t page_size, Continuation c)
      -> std::future<Result<Song>>;

  auto GetDump(std::size_t page_size) -> std::future<Result<std::string>>;
  auto GetMoreDump(std::size_t page_size, Continuation c)
      -> std::future<Result<std::string>>;

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

 private:
  std::unique_ptr<leveldb::DB> db_;
  std::unique_ptr<leveldb::Cache> cache_;

  Database(leveldb::DB* db, leveldb::Cache* cache);

  auto dbMintNewSongId() -> SongId;
  auto dbEntomb(SongId song, uint64_t hash) -> void;

  auto dbPutSongData(const SongData& s) -> void;
  auto dbGetSongData(SongId id) -> std::optional<SongData>;
  auto dbPutHash(const uint64_t& hash, SongId i) -> void;
  auto dbGetHash(const uint64_t& hash) -> std::optional<SongId>;
  auto dbPutSong(SongId id, const std::string& path, const uint64_t& hash)
      -> void;

  template <typename T>
  using Parser = std::function<std::optional<T>(const leveldb::Slice& key,
                                                const leveldb::Slice& value)>;

  template <typename T>
  auto Query(const leveldb::Slice& prefix,
             std::size_t max_results,
             Parser<T> parser) -> Result<T> {
    leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());
    it->Seek(prefix);
    return Query(it, max_results, parser);
  }

  template <typename T>
  auto Query(leveldb::Iterator* it, std::size_t max_results, Parser<T> parser)
      -> Result<T> {
    auto results = std::make_unique<std::vector<T>>();
    for (std::size_t i = 0; i < max_results && it->Valid(); i++) {
      std::optional<T> r = std::invoke(parser, it->key(), it->value());
      if (r) {
        results->push_back(*r);
      }
      it->Next();
    }
    return {std::move(results), std::unique_ptr<leveldb::Iterator>(it)};
  }
};

}  // namespace database
