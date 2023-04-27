#pragma once

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
#include "result.hpp"

namespace database {

struct Artist {
  std::string name;
};

struct Album {
  std::string name;
};

typedef uint64_t SongId_t;

struct Song {
  std::string title;
  uint64_t id;
};

struct SongMetadata {};

typedef std::unique_ptr<leveldb::Iterator> Continuation;

template <typename T>
class Result {
 public:
  auto values() -> std::unique_ptr<std::vector<T>> {
    return std::move(values_);
  }
  auto continuation() -> Continuation { return std::move(c_); }
  auto HasMore() -> bool { return c_->Valid(); }

  Result(std::unique_ptr<std::vector<T>> values, Continuation c)
      : values_(std::move(values)), c_(std::move(c)) {}

  Result(Result&& other)
      : values_(std::move(other.values_)), c_(std::move(other.c_)) {}

  Result operator=(Result&& other) {
    return Result(other.values(), other.continuation());
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

  auto Populate() -> std::future<void>;

  auto GetArtists(std::size_t page_size) -> std::future<Result<Artist>>;
  auto GetMoreArtists(std::size_t page_size, Continuation c)
      -> std::future<Result<Artist>>;

  auto GetAlbums(std::size_t page_size, std::optional<Artist> artist)
      -> std::future<Result<Album>>;
  auto GetMoreAlbums(std::size_t page_size, Continuation c)
      -> std::future<Result<Album>>;

  auto GetSongs(std::size_t page_size) -> std::future<Result<Song>>;
  auto GetSongs(std::size_t page_size, std::optional<Artist> artist)
      -> std::future<Result<Song>>;
  auto GetSongs(std::size_t page_size,
                std::optional<Artist> artist,
                std::optional<Album> album) -> std::future<Result<Song>>;
  auto GetMoreSongs(std::size_t page_size, Continuation c)
      -> std::future<Result<Song>>;

  auto GetSongIds(std::optional<Artist> artist, std::optional<Album> album)
      -> std::future<std::vector<SongId_t>>;
  auto GetSongFilePath(SongId_t id) -> std::future<std::optional<std::string>>;
  auto GetSongMetadata(SongId_t id) -> std::future<std::optional<SongMetadata>>;

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

 private:
  std::unique_ptr<leveldb::DB> db_;
  std::unique_ptr<leveldb::Cache> cache_;

  Database(leveldb::DB* db, leveldb::Cache* cache);

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
