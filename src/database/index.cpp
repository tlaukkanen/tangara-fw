/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "index.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <variant>

#include "collation.hpp"
#include "esp_log.h"
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
      return "0";
    case Tag::kDuration:
    default:
      return {};
  }
}

auto Index(locale::ICollator& collator, const IndexInfo& info, const Track& t)
    -> std::vector<std::pair<IndexKey, std::pmr::string>> {
  std::vector<std::pair<IndexKey, std::pmr::string>> out;
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
    Tag component = info.components.at(i);
    // Fill in the text for this depth.
    std::pmr::string value;

    if (component == Tag::kAlbumTrack) {
      // Track numbers are a special case, since they're numbers rather than
      // text.
      auto pmr_num = t.tags().at(component).value_or("0");
      // std::pmr continues to be a true disappointment.
      std::string raw_num{pmr_num.data(), pmr_num.size()};
      uint32_t num = std::stoi(raw_num);
      key.item = std::pmr::string{reinterpret_cast<char*>(&num), 4};
    } else {
      auto text = t.tags().at(component);
      if (text) {
        std::pmr::string orig = *text;
        auto xfrm = collator.Transform({orig.data(), orig.size()});
        key.item = {xfrm.data(), xfrm.size()};
        value = *text;
      } else {
        key.item = {};
        value = missing_component_text(t, info.components.at(i)).value_or("");
      }
    }

    // If this is the last component, then we should also fill in the track id
    // and title.
    if (i == info.components.size() - 1) {
      key.track = t.data().id;
      value = t.TitleOrFilename();
    }

    out.push_back(std::make_pair(key, value));

    // If there are more components after this, then we need to finish by
    // narrowing the header with the current title.
    if (i < info.components.size() - 1) {
      key.header = ExpandHeader(key.header, key.item);
    }
  }
  return out;
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
