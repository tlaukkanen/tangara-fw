#include "database.hpp"

#include "esp_log.h"
#include "ff.h"
#include "leveldb/cache.h"

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

FRESULT scan_files(const std::string &path) {
    FRESULT res;
    FF_DIR dir;
    static FILINFO fno;

    res = f_opendir(&dir, path.c_str());
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (fno.fname[0] == '.') continue;
            if (fno.fattrib & AM_DIR) {
                std::string new_path = path + "/" + fno.fname;
                res = scan_files(new_path);
                if (res != FR_OK) break;
            } else {
              ESP_LOGI(kTag, "found %s", fno.fname);
            }
        }
        f_closedir(&dir);
    }

    return res;
}

auto Database::Initialise() -> void {
  // TODO(jacqueline): Abstractions lol
  scan_files("/");
}

auto Database::Update() -> void {
  // TODO(jacqueline): Incremental updates!
}

}  // namespace database
