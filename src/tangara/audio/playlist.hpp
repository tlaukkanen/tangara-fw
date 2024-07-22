
/*
 * Copyright 2024 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <string>
#include <variant>
#include "database/database.hpp"
#include "database/track.hpp"
#include "ff.h"

namespace audio {

/*
 * Owns and manages a playlist file.
 * Each line in the playlist file is the absolute filepath of the track to play.
 * In order to avoid mapping to byte offsets, each line must contain only a
 * filepath (ie, no comments are supported). This limitation may be removed
 * later if benchmarks show that the file can be quickly scanned from 'bookmark'
 * offsets. This is a subset of the m3u format and ideally will be
 * import/exportable to and from this format, to better support playlists from
 * beets import and other music management software.
 */
class Playlist {
 public:
  Playlist(std::string playlistFilepath);
  ~Playlist();
  using Item =
      std::variant<database::TrackId, database::TrackIterator, std::string>;
  auto open() -> bool;
  auto currentPosition() const -> size_t;
  auto size() const -> size_t;
  auto append(Item i) -> void;
  auto skipTo(size_t position) -> void;
  auto next() -> void;
  auto prev() -> void;
  auto value() const -> std::string;
  auto clear() -> bool;
  auto atEnd() -> bool;
  auto filepath() -> std::string;

 private:
  std::string filepath_;
  std::mutex mutex_;
  size_t total_size_;
  size_t pos_;
  FIL file_;
  std::string current_value_;


  std::pmr::vector<FSIZE_t> offset_cache_; // List of offsets determined by sample size;
  /*
  * How many tracks per offset saved (ie, a value of 100 means every 100 tracks the file offset is saved)
  * This speeds up searches, especially in the case of shuffling a lot of tracks.
  */
  const uint32_t sample_size_; 

  auto consumeAndCount(ssize_t upto) -> bool;
  auto advanceBy(ssize_t amt) -> bool;
};

}  // namespace audio