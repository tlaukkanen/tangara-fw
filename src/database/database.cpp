#include "database.hpp"
#include <functional>

#include "esp_log.h"
#include "ff.h"
#include "leveldb/cache.h"

#include "db_task.hpp"
#include "env_esp.hpp"
#include "file_gatherer.hpp"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "result.hpp"
#include "tag_processor.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
static const char* kTag = "DB";

static std::atomic<bool> sIsDbOpen(false);

auto Database::Open() -> cpp::result<Database*, DatabaseError> {
  // TODO(jacqueline): Why isn't compare_and_exchange_* available?
  if (sIsDbOpen.exchange(true)) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  if (!StartDbTask()) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  return RunOnDbTask<cpp::result<Database*, DatabaseError>>(
             []() -> cpp::result<Database*, DatabaseError> {
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
                 ESP_LOGE(kTag, "failed to open db, status %s",
                          status.ToString().c_str());
                 return cpp::fail(FAILED_TO_OPEN);
               }

               ESP_LOGI(kTag, "Database opened successfully");
               return new Database(db, cache);
             })
      .get();
}

Database::Database(leveldb::DB* db, leveldb::Cache* cache)
    : db_(db), cache_(cache) {}

Database::~Database() {
  QuitDbTask();
  sIsDbOpen.store(false);
}

template <typename Parser>
auto IterateAndParse(leveldb::Iterator* it, std::size_t limit, Parser p)
    -> void {
  for (int i = 0; i < limit; i++) {
    if (!it->Valid()) {
      break;
    }
    std::invoke(p, it->key(), it->value());
    it->Next();
  }
}

auto Database::Populate() -> std::future<void> {
  return RunOnDbTask<void>([&]() -> void {
    leveldb::WriteOptions opt;
    opt.sync = true;
    FindFiles("", [&](const std::string& path) {
      ESP_LOGI(kTag, "considering %s", path.c_str());
      FileInfo info;
      if (GetInfo(path, &info)) {
        ESP_LOGI(kTag, "added as '%s'", info.title.c_str());
        db_->Put(opt, "title:" + info.title, path);
      }
    });
    db_->Put(opt, "title:coolkeywithoutval", leveldb::Slice());
  });
}

auto parse_song(const leveldb::Slice& key, const leveldb::Slice& val)
    -> std::optional<Song> {
  Song s;
  s.title = key.ToString();
  return s;
}

auto Database::GetSongs(std::size_t page_size) -> std::future<Result<Song>> {
  return RunOnDbTask<Result<Song>>([=, this]() -> Result<Song> {
    return Query<Song>("title:", page_size, &parse_song);
  });
}

auto Database::GetMoreSongs(std::size_t page_size, Continuation c)
    -> std::future<Result<Song>> {
  leveldb::Iterator* it = c.release();
  return RunOnDbTask<Result<Song>>([=, this]() -> Result<Song> {
    return Query<Song>(it, page_size, &parse_song);
  });
}

}  // namespace database
