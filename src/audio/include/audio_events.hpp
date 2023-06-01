/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "tinyfsm.hpp"

#include "song.hpp"

namespace audio {

struct PlayFile : tinyfsm::Event {
  std::string filename;
};

struct PlaySong : tinyfsm::Event {
  database::SongId id;
  std::optional<database::SongData> data;
  std::optional<database::SongTags> tags;
};

}  // namespace audio
