/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <string>

#include "leveldb/db.h"
#include "leveldb/slice.h"

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
auto CreateDataPrefix() -> OwningSlice;

/* Creates a data key for a track with the specified id. */
auto CreateDataKey(const TrackId& id) -> OwningSlice;

/*
 * Encodes a TrackData instance into bytes, in preparation for storing it within
 * the database. This encoding is consistent, and will remain stable over time.
 */
auto CreateDataValue(const TrackData& track) -> OwningSlice;

/*
 * Parses bytes previously encoded via CreateDataValue back into a TrackData.
 * May return nullopt if parsing fails.
 */
auto ParseDataValue(const leveldb::Slice& slice) -> std::optional<TrackData>;

/* Creates a hash key for the specified hash. */
auto CreateHashKey(const uint64_t& hash) -> OwningSlice;

/*
 * Encodes a hash value (at this point just a track id) into bytes, in
 * preparation for storing within the database. This encoding is consistent, and
 * will remain stable over time.
 */
auto CreateHashValue(TrackId id) -> OwningSlice;

/*
 * Parses bytes previously encoded via CreateHashValue back into a track id. May
 * return nullopt if parsing fails.
 */
auto ParseHashValue(const leveldb::Slice&) -> std::optional<TrackId>;

/* Encodes a TrackId as bytes. */
auto TrackIdToBytes(TrackId id) -> OwningSlice;

/*
 * Converts a track id encoded via TrackIdToBytes back into a TrackId. May
 * return nullopt if parsing fails.
 */
auto BytesToTrackId(const std::string& bytes) -> std::optional<TrackId>;

}  // namespace database
