/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <memory>
#include <string>

#include "tinyfsm.hpp"

#include "track.hpp"
#include "track_queue.hpp"
#include "types.hpp"

namespace audio {

struct Track {
  std::shared_ptr<database::TrackTags> tags;
  std::shared_ptr<database::TrackData> db_info;

  uint32_t duration;
  uint32_t bitrate_kbps;
  codecs::StreamType encoding;
};

struct PlaybackStarted : tinyfsm::Event {};

struct PlaybackUpdate : tinyfsm::Event {
  uint32_t seconds_elapsed;
  std::shared_ptr<Track> track;
};

struct PlaybackFinished : tinyfsm::Event {};

struct QueueUpdate : tinyfsm::Event {
  bool current_changed;
};

struct PlayFile : tinyfsm::Event {
  std::string filename;
};

struct StepUpVolume : tinyfsm::Event {};
struct StepDownVolume : tinyfsm::Event {};
struct VolumeChanged : tinyfsm::Event {
  uint_fast8_t percent;
  int db;
};
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
