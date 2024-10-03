
/*
 * Copyright 2024 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>
#include <variant>

#include "ff.h"

#include "database/database.hpp"
#include "database/track.hpp"

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
  Playlist(const std::string& playlistFilepath);
  virtual ~Playlist();
  using Item =
      std::variant<database::TrackId, database::TrackIterator, std::string>;
  virtual auto open() -> bool;

  auto filepath() const -> std::string;
  auto currentPosition() const -> size_t;
  auto size() const -> size_t;
  auto value() const -> std::string;
  auto atEnd() const -> bool;

  auto next() -> void;
  auto prev() -> void;
  auto skipTo(size_t position) -> void;

  auto serialiseCache() -> bool;
  auto deserialiseCache() -> bool;

 protected:
  const std::string filepath_;

  mutable std::mutex mutex_;
  size_t total_size_;
  ssize_t pos_;

  FIL file_;
  bool file_open_;
  bool file_error_;

  std::string current_value_;

  /* List of offsets determined by sample size */
  std::pmr::vector<FSIZE_t> offset_cache_;

  /*
   * How many tracks per offset saved (ie, a value of 100 means every 100 tracks
   * the file offset is saved) This speeds up searches, especially in the case
   * of shuffling a lot of tracks.
   */
  const uint32_t sample_size_;

 protected:
  auto skipToLocked(size_t position) -> void;
  auto countItems() -> void;
  auto advanceBy(ssize_t amt) -> bool;
  auto nextItem(std::span<TCHAR>) -> std::optional<std::string_view>;
  auto skipToWithoutCache(size_t position) -> void;
};

class MutablePlaylist : public Playlist {
 public:
  MutablePlaylist(const std::string& playlistFilepath);
  auto open() -> bool override;

  auto clear() -> bool;
  auto append(Item i) -> void;

 private:
  auto clearLocked() -> bool;
};

}  // namespace audio
