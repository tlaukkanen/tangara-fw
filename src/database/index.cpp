/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "index.hpp"

#include <cstdint>
#include <variant>

#include "komihash.h"
#include "leveldb/write_batch.h"

#include "records.hpp"

namespace database {

const IndexInfo kAlbumsByArtist{
    .id = 1,
    .name = "Albums by Artist",
    .components = {Tag::kArtist, Tag::kAlbum, Tag::kAlbumTrack},
};

const IndexInfo kTracksByGenre{
    .id = 2,
    .name = "Tracks by Genre",
    .components = {Tag::kGenre, Tag::kTitle},
};

const IndexInfo kAllTracks{
    .id = 3,
    .name = "All Tracks",
    .components = {Tag::kTitle},
};

const IndexInfo kAllAlbums{
    .id = 4,
    .name = "All Albums",
    .components = {Tag::kAlbum, Tag::kAlbumTrack},
};

static auto missing_component_text(const Track& track, Tag tag)
    -> std::optional<std::pmr::string> {
  switch (tag) {
    case Tag::kArtist:
      return "Unknown Artist";
    case Tag::kAlbum:
      return "Unknown Album";
    case Tag::kGenre:
      return "Unknown Genre";
    case Tag::kTitle:
      return track.TitleOrFilename();
    case Tag::kAlbumTrack:
      return "0000";
    case Tag::kDuration:
    default:
      return {};
  }
}

auto Index(const IndexInfo& info, const Track& t, leveldb::WriteBatch* batch)
    -> bool {
  IndexKey key{
      .header{
          .id = info.id,
          .depth = 0,
          .components_hash = 0,
      },
      .item = {},
      .track = {},
  };

  auto& col = std::use_facet<std::collate<char>>(std::locale());

  for (std::uint8_t i = 0; i < info.components.size(); i++) {
    // Fill in the text for this depth.
    auto text = t.tags().at(info.components.at(i));
    std::pmr::string value;
    if (text) {
      std::pmr::string orig = *text;
      key.item = col.transform(&orig[0], &orig[0] + orig.size());
      value = *text;
    } else {
      key.item = {};
      value = missing_component_text(t, info.components.at(i)).value_or("");
    }

    // If this is the last component, then we should also fill in the track id
    // and title.
    if (i == info.components.size() - 1) {
      key.track = t.data().id();
      value = t.TitleOrFilename();
    }

    auto encoded = EncodeIndexKey(key);
    batch->Put(encoded.slice, {value.data(), value.size()});

    // If there are more components after this, then we need to finish by
    // narrowing the header with the current title.
    if (i < info.components.size() - 1) {
      key.header = ExpandHeader(key.header, key.item);
    }
  }
  return true;
}

auto ExpandHeader(const IndexKey::Header& header,
                  const std::optional<std::pmr::string>& component)
    -> IndexKey::Header {
  IndexKey::Header ret{header};
  ret.depth++;
  if (component) {
    ret.components_hash =
        komihash(component->data(), component->size(), ret.components_hash);
  } else {
    ret.components_hash = komihash(NULL, 0, ret.components_hash);
  }
  return ret;
}

}  // namespace database
