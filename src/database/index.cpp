/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "index.hpp"
#include <stdint.h>
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

  for (std::uint8_t i = 0; i < info.components.size(); i++) {
    // Fill in the text for this depth.
    auto text = t.tags().at(info.components.at(i));
    if (text) {
      key.item = *text;
    } else {
      key.item = {};
    }

    // If this is the last component, then we should also fill in the track id
    // and title.
    std::optional<std::string> title;
    if (i == info.components.size() - 1) {
      key.track = t.data().id();
      if (info.components.at(i) != Tag::kTitle) {
        title = t.TitleOrFilename();
      }
    } else {
      key.track = {};
    }

    auto encoded = EncodeIndexKey(key);
    batch->Put(encoded.slice, title.value_or(""));

    // If there are more components after this, then we need to finish by
    // narrowing the header with the current title.
    if (i < info.components.size() - 1) {
      key.header = ExpandHeader(key.header, key.item);
    }
  }
  return true;
}

auto ExpandHeader(const IndexKey::Header& header,
                  const std::optional<std::string>& component)
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
