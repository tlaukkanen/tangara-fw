/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track.hpp"

#include <komihash.h>
#include <sys/_stdint.h>

#include "memory_resource.hpp"

namespace database {

auto TrackTags::set(const Tag& key, const std::pmr::string& val) -> void {
  tags_[key] = val;
}

auto TrackTags::at(const Tag& key) const -> std::optional<std::pmr::string> {
  if (tags_.contains(key)) {
    return tags_.at(key);
  }
  return {};
}

auto TrackTags::operator[](const Tag& key) const
    -> std::optional<std::pmr::string> {
  return at(key);
}

/* Helper function to update a komihash stream with a std::pmr::string. */
auto HashString(komihash_stream_t* stream, const std::pmr::string& str)
    -> void {
  komihash_stream_update(stream, str.c_str(), str.length());
}

/*
 * Uses a komihash stream to incrementally hash tags. This lowers the function's
 * memory footprint a little so that it's safe to call from any stack.
 */
auto TrackTags::Hash() const -> uint64_t {
  // TODO(jacqueline): this function doesn't work very well for tracks with no
  // tags at all.
  komihash_stream_t stream;
  komihash_stream_init(&stream, 0);

  HashString(&stream, at(Tag::kTitle).value_or(""));
  HashString(&stream, at(Tag::kArtist).value_or(""));
  HashString(&stream, at(Tag::kAlbum).value_or(""));
  HashString(&stream, at(Tag::kAlbumTrack).value_or(""));

  return komihash_stream_final(&stream);
}

auto Track::TitleOrFilename() const -> std::pmr::string {
  auto title = tags().at(Tag::kTitle);
  if (title) {
    return *title;
  }
  auto start = data().filepath.find_last_of('/');
  if (start == std::pmr::string::npos) {
    return data().filepath;
  }
  return data().filepath.substr(start);
}

}  // namespace database
