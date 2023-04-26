#pragma once

#include <string>
#include <memory>
#include <optional>

#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "result.hpp"

namespace database {

class Iterator;

class Database {
 public:
  enum DatabaseError {
    FAILED_TO_OPEN,
  };
  static auto Open() -> cpp::result<Database*, DatabaseError>;

  ~Database();

  auto Initialise() -> void;
  auto ByTitle() -> Iterator;

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

 private:
  std::unique_ptr<leveldb::DB> db_;
  std::unique_ptr<leveldb::Cache> cache_;

  Database(leveldb::DB* db, leveldb::Cache* cache);
};

class Iterator {
  public:
    explicit Iterator(leveldb::Iterator *it) : it_(it) {}

    auto Next() -> std::optional<std::string>;

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

  private:
    std::unique_ptr<leveldb::Iterator> it_;
};

}  // namespace database
