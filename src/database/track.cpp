/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track.hpp"

#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <sstream>
#include <string>

#include "esp_log.h"
#include "komihash.h"

#include "memory_resource.hpp"
#include "span.hpp"

namespace database {

static constexpr char kGenreDelimiters[] = ",;";

auto tagName(Tag t) -> std::string {
  switch (t) {
    case Tag::kTitle:
      return "title";
    case Tag::kArtist:
      return "artist";
    case Tag::kAlbum:
      return "album";
    case Tag::kAlbumArtist:
      return "album_artist";
    case Tag::kDisc:
      return "disc";
    case Tag::kTrack:
      return "track";
    case Tag::kAlbumOrder:
      return "album_order";
    case Tag::kGenres:
      return "genre";
  }
  return "";
}

auto tagHash(const TagValue& t) -> uint64_t {
  return std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return static_cast<uint64_t>(0);
        } else if constexpr (std::is_same_v<T, std::pmr::string>) {
          return komihash(arg.data(), arg.size(), 0);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          return komihash(&arg, sizeof(arg), 0);
        } else if constexpr (std::is_same_v<
                                 T, cpp::span<const std::pmr::string>>) {
          komihash_stream_t hash;
          komihash_stream_init(&hash, 0);
          for (const auto& i : arg) {
            komihash_stream_update(&hash, i.data(), i.size());
          }
          return komihash_stream_final(&hash);
        }
      },
      t);
  return 0;
}

auto tagToString(const TagValue& val) -> std::string {
  return std::visit(
      [&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return "";
        } else if constexpr (std::is_same_v<T, std::pmr::string>) {
          return {arg.data(), arg.size()};
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          return std::to_string(arg);
        } else if constexpr (std::is_same_v<
                                 T, cpp::span<const std::pmr::string>>) {
          std::ostringstream builder{};
          for (const auto& str : arg) {
            builder << std::string{str.data(), str.size()} << ",";
          }
          return builder.str();
        }
      },
      val);
  return "";
}

auto TrackTags::create() -> std::shared_ptr<TrackTags> {
  return std::allocate_shared<TrackTags,
                              std::pmr::polymorphic_allocator<TrackTags>>(
      &memory::kSpiRamResource);
}

template <typename T>
auto valueOrMonostate(std::optional<T> t) -> TagValue {
  if (t) {
    return *t;
  }
  return std::monostate{};
}

auto TrackTags::get(Tag t) const -> TagValue {
  switch (t) {
    case Tag::kTitle:
      return valueOrMonostate(title_);
    case Tag::kArtist:
      return valueOrMonostate(artist_);
    case Tag::kAlbum:
      return valueOrMonostate(album_);
    case Tag::kAlbumArtist:
      return valueOrMonostate(album_artist_);
    case Tag::kDisc:
      return valueOrMonostate(disc_);
    case Tag::kTrack:
      return valueOrMonostate(track_);
    case Tag::kAlbumOrder:
      return albumOrder();
    case Tag::kGenres:
      return genres_;
  }
  return std::monostate{};
}

auto TrackTags::set(Tag t, std::string_view v) -> void {
  switch (t) {
    case Tag::kTitle:
      title(v);
      break;
    case Tag::kArtist:
      artist(v);
      break;
    case Tag::kAlbum:
      album(v);
      break;
    case Tag::kAlbumArtist:
      albumArtist(v);
      break;
    case Tag::kDisc:
      disc(v);
      break;
    case Tag::kTrack:
      track(v);
      break;
    case Tag::kAlbumOrder:
      // This tag is derices from disc and track, and so it can't be set.
      break;
    case Tag::kGenres:
      genres(v);
      break;
  }
}

auto TrackTags::allPresent() const -> std::vector<Tag> {
  std::vector<Tag> out;
  auto add_if_present = [&](Tag t, auto opt) {
    if (opt) {
      out.push_back(t);
    }
  };
  add_if_present(Tag::kTitle, title_);
  add_if_present(Tag::kArtist, artist_);
  add_if_present(Tag::kAlbum, album_);
  add_if_present(Tag::kAlbumArtist, album_artist_);
  add_if_present(Tag::kDisc, disc_);
  add_if_present(Tag::kTrack, track_);
  add_if_present(Tag::kGenres, !genres_.empty());
  return out;
}

auto TrackTags::title() const -> const std::optional<std::pmr::string>& {
  return title_;
}

auto TrackTags::title(std::string_view s) -> void {
  title_ = s;
}

auto TrackTags::artist() const -> const std::optional<std::pmr::string>& {
  return artist_;
}

auto TrackTags::artist(std::string_view s) -> void {
  artist_ = s;
}

auto TrackTags::album() const -> const std::optional<std::pmr::string>& {
  return album_;
}

auto TrackTags::album(std::string_view s) -> void {
  album_ = s;
}

auto TrackTags::albumArtist() const -> const std::optional<std::pmr::string>& {
  return album_artist_;
}

auto TrackTags::albumArtist(std::string_view s) -> void {
  album_artist_ = s;
}

auto TrackTags::disc() const -> const std::optional<uint8_t>& {
  return disc_;
}

auto TrackTags::disc(const std::string_view s) -> void {
  disc_ = std::strtol(s.data(), nullptr, 10);
}

auto TrackTags::track() const -> const std::optional<uint16_t>& {
  return track_;
}

auto TrackTags::track(const std::string_view s) -> void {
  track_ = std::strtol(s.data(), nullptr, 10);
}

auto TrackTags::albumOrder() const -> uint32_t {
  return (disc_.value_or(0) << 16) | track_.value_or(0);
}

auto TrackTags::genres() const -> cpp::span<const std::pmr::string> {
  return genres_;
}

auto TrackTags::genres(const std::string_view s) -> void {
  genres_.clear();
  std::string src = {s.data(), s.size()};
  char* token = std::strtok(src.data(), kGenreDelimiters);

  auto trim_and_add = [this](std::string_view s) {
    std::string copy = {s.data(), s.size()};

    // Trim the left
    copy.erase(copy.begin(),
               std::find_if(copy.begin(), copy.end(), [](unsigned char ch) {
                 return !std::isspace(ch);
               }));

    // Trim the right
    copy.erase(std::find_if(copy.rbegin(), copy.rend(),
                            [](unsigned char ch) { return !std::isspace(ch); })
                   .base(),
               copy.end());

    // Ignore empty strings.
    if (!copy.empty()) {
      genres_.push_back({copy.data(), copy.size()});
    }
  };

  if (token == NULL) {
    // No delimiters found in the input. Treat this as a single genre.
    trim_and_add(s);
  } else {
    while (token != NULL) {
      // Add tokens until no more delimiters found.
      trim_and_add(token);
      token = std::strtok(NULL, kGenreDelimiters);
    }
  }
}

/*
 * Uses a komihash stream to incrementally hash tags. This lowers the
 * function's memory footprint a little so that it's safe to call from any
 * stack.
 */
auto TrackTags::Hash() const -> uint64_t {
  // TODO(jacqueline): this function doesn't work very well for tracks with no
  // tags at all.
  komihash_stream_t stream;
  komihash_stream_init(&stream, 0);

  auto add = [&](const uint64_t& h) {
    komihash_stream_update(&stream, &h, sizeof(h));
  };

  add(tagHash(get(Tag::kTitle)));
  add(tagHash(get(Tag::kArtist)));
  add(tagHash(get(Tag::kAlbum)));
  add(tagHash(get(Tag::kAlbumArtist)));

  // TODO: Should we be including this?
  add(tagHash(get(Tag::kAlbumOrder)));

  return komihash_stream_final(&stream);
}

auto Track::TitleOrFilename() const -> std::pmr::string {
  auto title = tags().title();
  if (title) {
    return *title;
  }
  auto start = data().filepath.find_last_of('/');
  if (start == std::pmr::string::npos) {
    return data().filepath;
  }
  return data().filepath.substr(start + 1);
}
}  // namespace database
