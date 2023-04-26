#include "database.hpp"

#include "esp_log.h"
#include "ff.h"
#include "leveldb/cache.h"

#include "file_gatherer.hpp"
#include "leveldb/iterator.h"
#include "leveldb/slice.h"
#include "tag_processor.hpp"
#include "env_esp.hpp"
#include "leveldb/options.h"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
static const char *kTag = "DB";

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
    ESP_LOGE(kTag, "failed to open db, status %s", status.ToString().c_str());
    return cpp::fail(FAILED_TO_OPEN);
  }

  return new Database(db, cache);
}

Database::Database(leveldb::DB* db, leveldb::Cache* cache)
    : db_(db), cache_(cache) {}

Database::~Database() {}

auto Database::Initialise() -> void {
  leveldb::WriteOptions opt;
  opt.sync = true;
  FindFiles("", [&](const std::string &path) {
      ESP_LOGI(kTag, "considering %s", path.c_str());
      FileInfo info;
      if (GetInfo(path, &info)) {
        ESP_LOGI(kTag, "added as '%s'", info.title.c_str());
        db_->Put(opt, "title:" + info.title, path);
      }
  });
  db_->Put(opt, "title:coolkeywithoutval", leveldb::Slice());
}

auto Database::ByTitle() -> Iterator {
  leveldb::Iterator *it = db_->NewIterator(leveldb::ReadOptions());
  it->Seek("title:");
  while (it->Valid()) {
    ESP_LOGI(kTag, "%s : %s", it->key().ToString().c_str(), it->value().ToString().c_str());
    it->Next();
  }
  return Iterator(it);
}

auto Iterator::Next() -> std::optional<std::string> {
  if (!it_->Valid()) {
    return {};
  }
  std::string ret = it_->key().ToString();
  it_->Next();
  return ret;
}

}  // namespace database
