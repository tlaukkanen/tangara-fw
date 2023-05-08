#pragma once

#include <stdint.h>
#include <cstdint>
#include <optional>
#include <string>

#include "leveldb/db.h"
#include "span.hpp"

namespace database {

typedef uint32_t SongId;

enum Encoding { ENC_UNSUPPORTED, ENC_MP3 };

struct SongTags {
  Encoding encoding;
  std::optional<std::string> title;
  std::optional<std::string> artist;
  std::optional<std::string> album;
  auto Hash() const -> uint64_t;
};

auto ReadAndParseTags(const std::string& path, SongTags* out) -> bool;

class SongData {
 private:
  const SongId id_;
  const std::string filepath_;
  const uint64_t tags_hash_;
  const uint32_t play_count_;
  const bool is_tombstoned_;

 public:
  SongData(SongId id, const std::string& path, uint64_t hash)
      : id_(id),
        filepath_(path),
        tags_hash_(hash),
        play_count_(0),
        is_tombstoned_(false) {}
  SongData(SongId id,
           const std::string& path,
           uint64_t hash,
           uint32_t play_count,
           bool is_tombstoned)
      : id_(id),
        filepath_(path),
        tags_hash_(hash),
        play_count_(play_count),
        is_tombstoned_(is_tombstoned) {}

  auto id() const -> SongId { return id_; }
  auto filepath() const -> std::string { return filepath_; }
  auto play_count() const -> uint32_t { return play_count_; }
  auto tags_hash() const -> uint64_t { return tags_hash_; }
  auto is_tombstoned() const -> bool { return is_tombstoned_; }

  auto UpdateHash(uint64_t new_hash) const -> SongData;
  auto Entomb() const -> SongData;
  auto Exhume(const std::string& new_path) const -> SongData;
};

class Song {
 public:
  Song(SongData data, SongTags tags) : data_(data), tags_(tags) {}

  auto data() -> const SongData& { return data_; }
  auto tags() -> const SongTags& { return tags_; }

 private:
  const SongData data_;
  const SongTags tags_;
};

}  // namespace database
