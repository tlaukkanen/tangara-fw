/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <deque>
#include <memory>

#include "audio/audio_events.hpp"

namespace audio {

/*
 * Utility for tracking which track is currently being played (and how long it
 * has been playing for) based on counting samples that are put into and taken
 * out of the audio processor's output buffer.
 */
class StreamCues {
 public:
  StreamCues();

  /* Updates the current track given the new most recently played sample. */
  auto update(uint32_t sample) -> void;

  /* Returns the current track, and how long it has been playing for. */
  auto current() -> std::pair<std::shared_ptr<TrackInfo>, uint32_t>;

  auto hasStream() -> bool;

  auto addCue(std::shared_ptr<TrackInfo>, uint32_t start_at) -> void;

 private:
  uint32_t now_;

  struct Cue {
    std::shared_ptr<TrackInfo> track;
    uint32_t start_at;
  };

  std::optional<Cue> current_;
  std::deque<Cue> upcoming_;
};

}  // namespace audio
