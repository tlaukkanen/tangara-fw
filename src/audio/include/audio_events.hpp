/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <string>

#include "tinyfsm.hpp"

#include "track.hpp"
#include "track_queue.hpp"

namespace audio {

struct PlaybackStarted : tinyfsm::Event {};

struct PlaybackUpdate : tinyfsm::Event {
  uint32_t seconds_elapsed;
  uint32_t seconds_total;
};

struct PlaybackFinished : tinyfsm::Event {};

struct QueueUpdate : tinyfsm::Event {
  bool current_changed;
};

struct PlayFile : tinyfsm::Event {
  std::pmr::string filename;
};

struct StepUpVolume : tinyfsm::Event {};
struct StepDownVolume : tinyfsm::Event {};
struct VolumeChanged : tinyfsm::Event {};
struct ChangeMaxVolume : tinyfsm::Event {
  uint16_t new_max;
};

struct TogglePlayPause : tinyfsm::Event {};

struct OutputModeChanged : tinyfsm::Event {};

namespace internal {

struct InputFileOpened : tinyfsm::Event {};
struct InputFileClosed : tinyfsm::Event {};
struct InputFileFinished : tinyfsm::Event {};

struct AudioPipelineIdle : tinyfsm::Event {};

}  // namespace internal

}  // namespace audio
