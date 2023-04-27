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

template <typename T>
class DbResult {
 public:
  DbResult(const std::vector<T>& values, std::unique_ptr<leveldb::Iterator> it)
      : values_(values), it_(std::move(it)) {}
  auto values() -> std::vector<T> { return values_; }
  auto it() -> leveldb::Iterator* { return it_.release(); };

 private:
  std::vector<T> values_;
  std::unique_ptr<leveldb::Iterator> it_;
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

  auto GetArtists(std::size_t page_size) -> std::future<DbResult<Artist>>;
  auto GetMoreArtists(std::size_t page_size, DbResult<Artist> continuation)
      -> std::future<DbResult<Artist>>;

  auto GetAlbums(std::size_t page_size, std::optional<Artist> artist)
      -> std::future<DbResult<Album>>;
  auto GetMoreAlbums(std::size_t page_size, DbResult<Album> continuation)
      -> std::future<DbResult<Album>>;

  auto GetSongs(std::size_t page_size) -> std::future<DbResult<Song>>;
  auto GetSongs(std::size_t page_size, std::optional<Artist> artist)
      -> std::future<DbResult<Song>>;
  auto GetSongs(std::size_t page_size,
                std::optional<Artist> artist,
                std::optional<Album> album) -> std::future<DbResult<Song>>;
  auto GetMoreSongs(std::size_t page_size, DbResult<Song> continuation)
      -> std::future<DbResult<Song>>;

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
};

}  // namespace database
