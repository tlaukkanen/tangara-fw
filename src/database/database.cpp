#include "database.hpp"

#include "esp_log.h"
#include "leveldb/cache.h"

#include "env_esp.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;

auto Database::Open() -> cpp::result<Database*, DatabaseError> {
  leveldb::DB* db;
  leveldb::Cache* cache = leveldb::NewLRUCache(24 * 1024);
  leveldb::Options options;
  options.env = sEnv.env();
  options.create_if_missing = true;
  options.write_buffer_size = 48 * 1024;
  options.max_file_size = 32;
  options.block_cache = cache;
  options.block_size = 512;

  auto status = leveldb::DB::Open(options, "/.db", &db);
  if (!status.ok()) {
    delete cache;
    ESP_LOGE("DB", "failed to open db");
    return cpp::fail(FAILED_TO_OPEN);
  }

  return new Database(db, cache);
}

Database::Database(leveldb::DB* db, leveldb::Cache* cache)
    : db_(db), cache_(cache) {}

Database::~Database() {}

}  // namespace database
