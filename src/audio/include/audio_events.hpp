/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <string>

#include "tinyfsm.hpp"

#include "track.hpp"
#include "track_queue.hpp"

namespace audio {

struct PlaybackStarted : tinyfsm::Event {
  database::Track track;
};

struct PlaybackUpdate : tinyfsm::Event {
  uint32_t seconds_elapsed;
  uint32_t seconds_total;
};

struct QueueUpdate : tinyfsm::Event {
  bool current_changed;
};

struct PlayFile : tinyfsm::Event {
  std::string filename;
};

struct VolumeChanged : tinyfsm::Event {};

namespace internal {

struct InputFileOpened : tinyfsm::Event {};
struct InputFileClosed : tinyfsm::Event {};
struct InputFileFinished : tinyfsm::Event {};

struct AudioPipelineIdle : tinyfsm::Event {};

}  // namespace internal

}  // namespace audio
