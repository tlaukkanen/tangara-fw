/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track.hpp"

#include <komihash.h>
#include "shared_string.h"

namespace database {

auto TrackTags::set(const Tag& key, const std::string& val) -> void {
  tags_[key] = val;
}

auto TrackTags::at(const Tag& key) const -> std::optional<shared_string> {
  if (tags_.contains(key)) {
    return tags_.at(key);
  }
  return {};
}

auto TrackTags::operator[](const Tag& key) const
    -> std::optional<shared_string> {
  return at(key);
}

/* Helper function to update a komihash stream with a std::string. */
auto HashString(komihash_stream_t* stream, const std::string& str) -> void {
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

auto TrackData::UpdateHash(uint64_t new_hash) const -> TrackData {
  return TrackData(id_, filepath_, new_hash, play_count_, is_tombstoned_);
}

auto TrackData::Entomb() const -> TrackData {
  return TrackData(id_, filepath_, tags_hash_, play_count_, true);
}

auto TrackData::Exhume(const std::string& new_path) const -> TrackData {
  return TrackData(id_, new_path, tags_hash_, play_count_, false);
}

void swap(Track& first, Track& second) {
  Track temp = first;
  first = second;
  second = temp;
}

auto Track::TitleOrFilename() const -> shared_string {
  auto title = tags().at(Tag::kTitle);
  if (title) {
    return *title;
  }
  auto start = data().filepath().find_last_of('/');
  if (start == std::string::npos) {
    return data().filepath();
  }
  return data().filepath().substr(start);
}

}  // namespace database
