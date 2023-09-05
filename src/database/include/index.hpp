/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "leveldb/db.h"
#include "leveldb/slice.h"

#include "leveldb/write_batch.h"
#include "shared_string.h"
#include "track.hpp"

namespace database {

typedef uint8_t IndexId;

struct IndexInfo {
  // Unique id for this index
  IndexId id;
  // Localised, user-friendly description of this index. e.g. "Albums by Artist"
  // or "All Tracks".
  std::string name;
  // Specifier for how this index breaks down the database.
  std::vector<Tag> components;
};

struct IndexKey {
  struct Header {
    // The index that this key was created for.
    IndexId id;
    // The number of components of IndexInfo that have already been filtered.
    // For example, if an index consists of { kGenre, kArtist }, and this key
    // represents an artist, then depth = 1.
    std::uint8_t depth;
    // The cumulative hash of all filtered components, in order. For example, if
    // an index consists of { kArtist, kAlbum, kTitle }, and we are at depth = 2
    // then this may contain hash(hash("Jacqueline"), "My Cool Album").
    std::uint64_t components_hash;
  };
  Header header;

  // The filterable / selectable item that this key represents. "Jacqueline" for
  // kArtist, "My Cool Album" for kAlbum, etc.
  std::optional<std::string> item;
  // If this is a leaf component, the track id for this record.
  // This could reasonably be the value for a record, but we keep it as a part
  // of the key to help with disambiguation.
  std::optional<TrackId> track;
};

auto Index(const IndexInfo&, const Track&, leveldb::WriteBatch*) -> bool;
auto ExpandHeader(const IndexKey::Header&, const std::optional<std::string>&)
    -> IndexKey::Header;

// Predefined indexes
// TODO(jacqueline): Make these defined at runtime! :)

extern const IndexInfo kAlbumsByArtist;
extern const IndexInfo kTracksByGenre;
extern const IndexInfo kAllTracks;
extern const IndexInfo kAllAlbums;

}  // namespace database
