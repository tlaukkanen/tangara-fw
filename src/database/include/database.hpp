#pragma once

#include <memory>

#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "result.hpp"

namespace database {

class Database {
 public:
  enum DatabaseError {
    FAILED_TO_OPEN,
  };
  static auto Open() -> cpp::result<Database*, DatabaseError>;

  ~Database();

  auto Initialise() -> void;
  auto Update() -> void;

 private:
  std::unique_ptr<leveldb::DB> db_;
  std::unique_ptr<leveldb::Cache> cache_;

  Database(leveldb::DB* db, leveldb::Cache* cache);
};

}  // namespace database
