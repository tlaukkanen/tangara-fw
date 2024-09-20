/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/index.hpp"
#include <sys/_stdint.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "collation.hpp"
#include "cppbor.h"
#include "esp_log.h"
#include "komihash.h"
#include "leveldb/write_batch.h"

#include "database/records.hpp"
#include "database/track.hpp"

namespace database {

[[maybe_unused]] static const char* kTag = "index";

const IndexInfo kAlbumsByArtist{
    .id = 1,
    .type = MediaType::kMusic,
    .name = "Albums by Artist",
    .components = {Tag::kAlbumArtist, Tag::kAlbum, Tag::kAlbumOrder},
};

const IndexInfo kTracksByGenre{
    .id = 2,
    .type = MediaType::kMusic,
    .name = "Tracks by Genre",
    .components = {Tag::kGenres, Tag::kTitle},
};

const IndexInfo kAllTracks{
    .id = 3,
    .type = MediaType::kMusic,
    .name = "All Tracks",
    .components = {Tag::kTitle},
};

const IndexInfo kAllAlbums{
    .id = 4,
    .type = MediaType::kMusic,
    .name = "All Albums",
    .components = {Tag::kAlbum, Tag::kAlbumOrder},
};

const IndexInfo kPodcasts{
    .id = 5,
    .type = MediaType::kPodcast,
    .name = "Podcasts",
    .components = {Tag::kTitle},
};

static auto titleOrFilename(const TrackData& data, const TrackTags& tags)
    -> std::pmr::string {
  auto title = tags.title();
  if (title) {
    return *title;
  }
  auto start = data.filepath.find_last_of('/');
  if (start == std::pmr::string::npos) {
    return data.filepath;
  }
  return data.filepath.substr(start + 1);
}

class Indexer {
 public:
  Indexer(locale::ICollator& collator,
          const IndexInfo& idx,
          const TrackData& data,
          const TrackTags& tags)
      : collator_(collator),
        index_(idx),
        track_data_(data),
        track_tags_(tags) {}

  auto index() -> std::vector<std::pair<IndexKey, std::string>>;

 private:
  auto handleLevel(const IndexKey::Header& header,
                   std::span<const Tag> components) -> void;

  auto handleItem(const IndexKey::Header& header,
                  std::variant<std::pmr::string, uint32_t> item,
                  std::span<const Tag> components) -> void;

  auto missing_value(Tag tag) -> TagValue {
    switch (tag) {
      case Tag::kTitle:
        return titleOrFilename(track_data_, track_tags_);
      case Tag::kArtist:
        return "Unknown Artist";
      case Tag::kAlbum:
        return "Unknown Album";
      case Tag::kAlbumArtist:
        return track_tags_.artist().value_or("Unknown Artist");
      case Tag::kGenres:
        return std::pmr::vector<std::pmr::string>{};
      case Tag::kDisc:
        return 0u;
      case Tag::kTrack:
        return 0u;
      case Tag::kAlbumOrder:
        return 0u;
    }
    return std::monostate{};
  }

  locale::ICollator& collator_;
  const IndexInfo index_;
  const TrackData& track_data_;
  const TrackTags& track_tags_;

  std::vector<std::pair<IndexKey, std::string>> out_;
};

auto Indexer::index() -> std::vector<std::pair<IndexKey, std::string>> {
  out_.clear();

  IndexKey::Header root_header{
      .id = index_.id,
      .depth = 0,
      .components_hash = 0,
  };
  handleLevel(root_header, index_.components);

  return out_;
}

auto Indexer::handleLevel(const IndexKey::Header& header,
                          std::span<const Tag> components) -> void {
  Tag component = components.front();
  TagValue value = track_tags_.get(component);
  if (std::holds_alternative<std::monostate>(value)) {
    value = missing_value(component);
  }

  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          ESP_LOGW(kTag, "dropping component without value: %s",
                   tagName(components.front()).c_str());
        } else if constexpr (std::is_same_v<T, std::pmr::string>) {
          handleItem(header, arg, components);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          handleItem(header, arg, components);
        } else if constexpr (std::is_same_v<
                                 T, std::span<const std::pmr::string>>) {
          for (const auto& i : arg) {
            handleItem(header, i, components);
          }
        }
      },
      value);
}

auto Indexer::handleItem(const IndexKey::Header& header,
                         std::variant<std::pmr::string, uint32_t> item,
                         std::span<const Tag> components) -> void {
  IndexKey key{
      .header = header,
      .item = {},
      .track = {},
  };
  std::string value;

  std::string item_text;
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::pmr::string>) {
          value = {arg.data(), arg.size()};
          auto xfrm = collator_.Transform(value);
          key.item = {xfrm.data(), xfrm.size()};
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          // CBOR's varint encoding actually works great for lexicographical
          // sorting.
          key.item = cppbor::Uint{arg}.toString();
        }
      },
      item);

  std::optional<IndexKey::Header> next_level;
  if (components.size() == 1) {
    value = titleOrFilename(track_data_, track_tags_);
    key.track = track_data_.id;
  } else {
    next_level = ExpandHeader(key.header, key.item);
  }

  out_.emplace_back(key, value);

  if (next_level) {
    handleLevel(*next_level, components.subspan(1));
  }
}

auto Index(locale::ICollator& collator,
           const IndexInfo& index,
           const TrackData& data,
           const TrackTags& tags)
    -> std::vector<std::pair<IndexKey, std::string>> {
  if (index.type != data.type) {
    return {};
  }
  Indexer indexer{collator, index, data, tags};
  return indexer.index();
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
