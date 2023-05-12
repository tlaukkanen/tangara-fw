#include "database.hpp"

#include <stdint.h>

#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>

#include "esp_log.h"
#include "ff.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"

#include "db_task.hpp"
#include "env_esp.hpp"
#include "file_gatherer.hpp"
#include "records.hpp"
#include "result.hpp"
#include "song.hpp"
#include "tag_parser.hpp"

namespace database {

static SingletonEnv<leveldb::EspEnv> sEnv;
static const char* kTag = "DB";

static const char kSongIdKey[] = "next_song_id";

static std::atomic<bool> sIsDbOpen(false);

static FileGathererImpl sFileGatherer;
static TagParserImpl sTagParser;

template <typename Parser>
auto IterateAndParse(leveldb::Iterator* it, std::size_t limit, Parser p)
    -> void {
  for (int i = 0; i < limit; i++) {
    if (!it->Valid()) {
      delete it;
      break;
    }
    std::invoke(p, it->key(), it->value());
    it->Next();
  }
}

auto Database::Open() -> cpp::result<Database*, DatabaseError> {
  return Open(&sFileGatherer, &sTagParser);
}

auto Database::Open(IFileGatherer* gatherer, ITagParser* parser)
    -> cpp::result<Database*, DatabaseError> {
  // TODO(jacqueline): Why isn't compare_and_exchange_* available?
  if (sIsDbOpen.exchange(true)) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  if (!StartDbTask()) {
    return cpp::fail(DatabaseError::ALREADY_OPEN);
  }

  return RunOnDbTask<cpp::result<Database*, DatabaseError>>(
             [=]() -> cpp::result<Database*, DatabaseError> {
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
               return new Database(db, cache, gatherer, parser);
             })
      .get();
}

auto Database::Destroy() -> void {
  leveldb::Options options;
  options.env = sEnv.env();
  leveldb::DestroyDB("/.db", options);
}

Database::Database(leveldb::DB* db,
                   leveldb::Cache* cache,
                   IFileGatherer* file_gatherer,
                   ITagParser* tag_parser)
    : db_(db),
      cache_(cache),
      file_gatherer_(file_gatherer),
      tag_parser_(tag_parser) {}

Database::~Database() {
  // Delete db_ first so that any outstanding background work finishes before
  // the background task is killed.
  delete db_;
  delete cache_;

  QuitDbTask();
  sIsDbOpen.store(false);
}

auto Database::Update() -> std::future<void> {
  return RunOnDbTask<void>([&]() -> void {
    // Stage 1: verify all existing songs are still valid.
    ESP_LOGI(kTag, "verifying existing songs");
    const leveldb::Snapshot* snapshot = db_->GetSnapshot();
    leveldb::ReadOptions read_options;
    read_options.fill_cache = false;
    read_options.snapshot = snapshot;
    leveldb::Iterator* it = db_->NewIterator(read_options);
    OwningSlice prefix = CreateDataPrefix();
    it->Seek(prefix.slice);
    while (it->Valid() && it->key().starts_with(prefix.slice)) {
      std::optional<SongData> song = ParseDataValue(it->value());
      if (!song) {
        // The value was malformed. Drop this record.
        ESP_LOGW(kTag, "dropping malformed metadata");
        db_->Delete(leveldb::WriteOptions(), it->key());
        it->Next();
        continue;
      }

      if (song->is_tombstoned()) {
        ESP_LOGW(kTag, "skipping tombstoned %lx", song->id());
        it->Next();
        continue;
      }

      SongTags tags;
      if (!tag_parser_->ReadAndParseTags(song->filepath(), &tags) ||
          tags.encoding == Encoding::kUnsupported) {
        // We couldn't read the tags for this song. Either they were
        // malformed, or perhaps the file is missing. Either way, tombstone
        // this record.
        ESP_LOGW(kTag, "entombing missing #%lx", song->id());
        dbPutSongData(song->Entomb());
        it->Next();
        continue;
      }

      uint64_t new_hash = tags.Hash();
      if (new_hash != song->tags_hash()) {
        // This song's tags have changed. Since the filepath is exactly the
        // same, we assume this is a legitimate correction. Update the
        // database.
        ESP_LOGI(kTag, "updating hash (%llx -> %llx)", song->tags_hash(),
                 new_hash);
        dbPutSongData(song->UpdateHash(new_hash));
        dbPutHash(new_hash, song->id());
      }

      it->Next();
    }
    delete it;
    db_->ReleaseSnapshot(snapshot);

    // Stage 2: search for newly added files.
    ESP_LOGI(kTag, "scanning for new songs");
    file_gatherer_->FindFiles("", [&](const std::string& path) {
      SongTags tags;
      if (!tag_parser_->ReadAndParseTags(path, &tags) ||
          tags.encoding == Encoding::kUnsupported) {
        // No parseable tags; skip this fiile.
        return;
      }

      // Check for any existing record with the same hash.
      uint64_t hash = tags.Hash();
      OwningSlice key = CreateHashKey(hash);
      std::optional<SongId> existing_hash;
      std::string raw_entry;
      if (db_->Get(leveldb::ReadOptions(), key.slice, &raw_entry).ok()) {
        existing_hash = ParseHashValue(raw_entry);
      }

      if (!existing_hash) {
        // We've never met this song before! Or we have, but the entry is
        // malformed. Either way, record this as a new song.
        SongId id = dbMintNewSongId();
        ESP_LOGI(kTag, "recording new 0x%lx", id);
        dbPutSong(id, path, hash);
        return;
      }

      std::optional<SongData> existing_data = dbGetSongData(*existing_hash);
      if (!existing_data) {
        // We found a hash that matches, but there's no data record? Weird.
        SongData new_data(*existing_hash, path, hash);
        dbPutSongData(new_data);
        return;
      }

      if (existing_data->is_tombstoned()) {
        ESP_LOGI(kTag, "exhuming song %lu", existing_data->id());
        dbPutSongData(existing_data->Exhume(path));
      } else if (existing_data->filepath() != path) {
        ESP_LOGW(kTag, "tag hash collision");
      }
    });
  });
}

auto Database::dbMintNewSongId() -> SongId {
  SongId next_id = 1;
  std::string val;
  auto status = db_->Get(leveldb::ReadOptions(), kSongIdKey, &val);
  if (status.ok()) {
    next_id = BytesToSongId(val).value_or(next_id);
  } else if (!status.IsNotFound()) {
    // TODO(jacqueline): Handle this more.
    ESP_LOGE(kTag, "failed to get next song id");
  }

  if (!db_->Put(leveldb::WriteOptions(), kSongIdKey,
                SongIdToBytes(next_id + 1).slice)
           .ok()) {
    ESP_LOGE(kTag, "failed to write next song id");
  }

  return next_id;
}

auto Database::dbEntomb(SongId id, uint64_t hash) -> void {
  OwningSlice key = CreateHashKey(hash);
  OwningSlice val = CreateHashValue(id);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to entomb #%llx (id #%lx)", hash, id);
  }
}

auto Database::dbPutSongData(const SongData& s) -> void {
  OwningSlice key = CreateDataKey(s.id());
  OwningSlice val = CreateDataValue(s);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to write data for #%lx", s.id());
  }
}

auto Database::dbGetSongData(SongId id) -> std::optional<SongData> {
  OwningSlice key = CreateDataKey(id);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key.slice, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for #%lx", id);
    return {};
  }
  return ParseDataValue(raw_val);
}

auto Database::dbPutHash(const uint64_t& hash, SongId i) -> void {
  OwningSlice key = CreateHashKey(hash);
  OwningSlice val = CreateHashValue(i);
  if (!db_->Put(leveldb::WriteOptions(), key.slice, val.slice).ok()) {
    ESP_LOGE(kTag, "failed to write hash for #%lx", i);
  }
}

auto Database::dbGetHash(const uint64_t& hash) -> std::optional<SongId> {
  OwningSlice key = CreateHashKey(hash);
  std::string raw_val;
  if (!db_->Get(leveldb::ReadOptions(), key.slice, &raw_val).ok()) {
    ESP_LOGW(kTag, "no key found for hash #%llx", hash);
    return {};
  }
  return ParseHashValue(raw_val);
}

auto Database::dbPutSong(SongId id,
                         const std::string& path,
                         const uint64_t& hash) -> void {
  dbPutSongData(SongData(id, path, hash));
  dbPutHash(hash, id);
}

auto parse_song(ITagParser* parser,
                const leveldb::Slice& key,
                const leveldb::Slice& value) -> std::optional<Song> {
  std::optional<SongData> data = ParseDataValue(value);
  if (!data) {
    return {};
  }
  SongTags tags;
  if (!parser->ReadAndParseTags(data->filepath(), &tags)) {
    return {};
  }
  return Song(*data, tags);
}

auto Database::GetSongs(std::size_t page_size) -> std::future<Result<Song>> {
  return RunOnDbTask<Result<Song>>([=, this]() -> Result<Song> {
    return Query<Song>(CreateDataPrefix().slice, page_size,
                       std::bind_front(&parse_song, tag_parser_));
  });
}

auto Database::GetMoreSongs(std::size_t page_size, Continuation c)
    -> std::future<Result<Song>> {
  leveldb::Iterator* it = c.release();
  return RunOnDbTask<Result<Song>>([=, this]() -> Result<Song> {
    return Query<Song>(it, page_size,
                       std::bind_front(&parse_song, tag_parser_));
  });
}

auto parse_dump(const leveldb::Slice& key, const leveldb::Slice& value)
    -> std::optional<std::string> {
  std::ostringstream stream;
  stream << "key: ";
  if (key.size() < 3 || key.data()[1] != '\0') {
    stream << key.ToString().c_str();
  } else {
    std::string str = key.ToString();
    for (size_t i = 0; i < str.size(); i++) {
      if (i == 0) {
        stream << str[i];
      } else if (i == 1) {
        stream << " / 0x";
      } else {
        stream << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(str[i]);
      }
    }
    for (std::size_t i = 2; i < str.size(); i++) {
    }
  }
  stream << "\tval: 0x";
  std::string str = value.ToString();
  for (int i = 0; i < value.size(); i++) {
    stream << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(str[i]);
  }
  return stream.str();
}

auto Database::GetDump(std::size_t page_size)
    -> std::future<Result<std::string>> {
  leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());
  it->SeekToFirst();
  return RunOnDbTask<Result<std::string>>([=, this]() -> Result<std::string> {
    return Query<std::string>(it, page_size, &parse_dump);
  });
}

auto Database::GetMoreDump(std::size_t page_size, Continuation c)
    -> std::future<Result<std::string>> {
  leveldb::Iterator* it = c.release();
  return RunOnDbTask<Result<std::string>>([=, this]() -> Result<std::string> {
    return Query<std::string>(it, page_size, &parse_dump);
  });
}

}  // namespace database
