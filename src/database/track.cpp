/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track.hpp"

#include <komihash.h>

namespace database {

/* Helper function to update a komihash stream with a std::string. */
auto HashString(komihash_stream_t* stream, std::string str) -> void {
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
  HashString(&stream, title.value_or(""));
  HashString(&stream, artist.value_or(""));
  HashString(&stream, album.value_or(""));
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

}  // namespace database
