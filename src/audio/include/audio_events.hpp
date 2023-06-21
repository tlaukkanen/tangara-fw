/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <string>

#include "tinyfsm.hpp"

#include "track.hpp"

namespace audio {

struct PlayFile : tinyfsm::Event {
  std::string filename;
};

struct PlayTrack : tinyfsm::Event {
  database::TrackId id;
  std::optional<database::TrackData> data;
};

struct PlaybackUpdate : tinyfsm::Event {
  uint32_t seconds_elapsed;
};

struct InputFileOpened : tinyfsm::Event {};
struct InputFileFinished : tinyfsm::Event {};
struct AudioPipelineIdle : tinyfsm::Event {};

}  // namespace audio
