#pragma once

#include <stdint.h>

#include <string>

#include "leveldb/db.h"
#include "leveldb/slice.h"

#include "song.hpp"

namespace database {

/*
 * Helper class for creating leveldb Slices bundled with the data they point to.
 * Slices are otherwise non-owning, which can make handling them post-creation
 * difficult.
 */
class OwningSlice {
 public:
  std::string data;
  leveldb::Slice slice;

  explicit OwningSlice(std::string d);
};

/*
 * Returns the prefix added to every SongData key. This can be used to iterate
 * over every data record in the database.
 */
auto CreateDataPrefix() -> OwningSlice;

/* Creates a data key for a song with the specified id. */
auto CreateDataKey(const SongId& id) -> OwningSlice;

/*
 * Encodes a SongData instance into bytes, in preparation for storing it within
 * the database. This encoding is consistent, and will remain stable over time.
 */
auto CreateDataValue(const SongData& song) -> OwningSlice;

/*
 * Parses bytes previously encoded via CreateDataValue back into a SongData. May
 * return nullopt if parsing fails.
 */
auto ParseDataValue(const leveldb::Slice& slice) -> std::optional<SongData>;

/* Creates a hash key for the specified hash. */
auto CreateHashKey(const uint64_t& hash) -> OwningSlice;

/*
 * Encodes a hash value (at this point just a song id) into bytes, in
 * preparation for storing within the database. This encoding is consistent, and
 * will remain stable over time.
 */
auto CreateHashValue(SongId id) -> OwningSlice;

/*
 * Parses bytes previously encoded via CreateHashValue back into a song id. May
 * return nullopt if parsing fails.
 */
auto ParseHashValue(const leveldb::Slice&) -> std::optional<SongId>;

/* Encodes a SongId as bytes. */
auto SongIdToBytes(SongId id) -> OwningSlice;

/*
 * Converts a song id encoded via SongIdToBytes back into a SongId. May return
 * nullopt if parsing fails.
 */
auto BytesToSongId(const std::string& bytes) -> std::optional<SongId>;

}  // namespace database
