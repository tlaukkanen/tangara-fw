/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <string>
#include <variant>
#include <vector>

#include "leveldb/db.h"
#include "leveldb/slice.h"

#include "index.hpp"
#include "shared_string.h"
#include "track.hpp"

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
 * Returns the prefix added to every TrackData key. This can be used to iterate
 * over every data record in the database.
 */
auto EncodeDataPrefix() -> OwningSlice;

/* Encodes a data key for a track with the specified id. */
auto EncodeDataKey(const TrackId& id) -> OwningSlice;

/*
 * Encodes a TrackData instance into bytes, in preparation for storing it within
 * the database. This encoding is consistent, and will remain stable over time.
 */
auto EncodeDataValue(const TrackData& track) -> OwningSlice;

/*
 * Parses bytes previously encoded via EncodeDataValue back into a TrackData.
 * May return nullopt if parsing fails.
 */
auto ParseDataValue(const leveldb::Slice& slice) -> std::optional<TrackData>;

/* Encodes a hash key for the specified hash. */
auto EncodeHashKey(const uint64_t& hash) -> OwningSlice;

/*
 * Encodes a hash value (at this point just a track id) into bytes, in
 * preparation for storing within the database. This encoding is consistent, and
 * will remain stable over time.
 */
auto EncodeHashValue(TrackId id) -> OwningSlice;

/*
 * Parses bytes previously encoded via EncodeHashValue back into a track id. May
 * return nullopt if parsing fails.
 */
auto ParseHashValue(const leveldb::Slice&) -> std::optional<TrackId>;

/* Encodes a prefix that matches all index keys, of all ids and depths. */
auto EncodeAllIndexesPrefix() -> OwningSlice;

/*
 */
auto EncodeIndexPrefix(const IndexKey::Header&) -> OwningSlice;

auto EncodeIndexKey(const IndexKey&) -> OwningSlice;
auto ParseIndexKey(const leveldb::Slice&) -> std::optional<IndexKey>;

/* Encodes a TrackId as bytes. */
auto TrackIdToBytes(TrackId id) -> OwningSlice;

/*
 * Converts a track id encoded via TrackIdToBytes back into a TrackId. May
 * return nullopt if parsing fails.
 */
auto BytesToTrackId(const std::string& bytes) -> std::optional<TrackId>;

}  // namespace database
