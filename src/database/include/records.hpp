#pragma once

#include <leveldb/db.h>
#include <stdint.h>
#include <string>

#include "leveldb/slice.h"
#include "song.hpp"

namespace database {

class OwningSlice {
 public:
  std::string data;
  leveldb::Slice slice;

  explicit OwningSlice(std::string d);
};

auto CreateDataPrefix() -> OwningSlice;
auto CreateDataKey(const SongId& id) -> OwningSlice;
auto CreateDataValue(const SongData& song) -> OwningSlice;
auto ParseDataValue(const leveldb::Slice& slice) -> std::optional<SongData>;

auto CreateHashKey(const uint64_t& hash) -> OwningSlice;
auto ParseHashValue(const leveldb::Slice&) -> std::optional<SongId>;
auto CreateHashValue(SongId id) -> OwningSlice;

auto SongIdToBytes(SongId id) -> OwningSlice;
auto BytesToSongId(const std::string& bytes) -> SongId;

}  // namespace database
