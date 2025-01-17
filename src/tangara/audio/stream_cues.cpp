/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/stream_cues.hpp"

#include <cstdint>
#include <memory>

namespace audio {

StreamCues::StreamCues() : now_(0) {}

auto StreamCues::update(uint32_t sample) -> void {
  if (sample < now_) {
    // The current time must have overflowed. Deal with any cues between now_
    // and UINT32_MAX, then proceed as normal.
    while (!upcoming_.empty() && upcoming_.front().start_at > now_) {
      current_ = upcoming_.front();
      upcoming_.pop_front();
    }
  }
  now_ = sample;

  // Advance the current queue until we've caught up.
  while (!upcoming_.empty() && upcoming_.front().start_at <= now_) {
    current_ = upcoming_.front();
    upcoming_.pop_front();
  }
}

auto StreamCues::addCue(std::shared_ptr<TrackInfo> track, uint32_t sample)
    -> void {
  if (sample == now_) {
    current_ = {track, now_};
  } else {
    upcoming_.push_back(Cue{
        .track = track,
        .start_at = sample,
    });
  }
}

auto StreamCues::current() -> std::pair<std::shared_ptr<TrackInfo>, uint32_t> {
  if (!current_) {
    return {};
  }

  uint32_t duration;
  if (now_ < current_->start_at) {
    // now_ overflowed since this track started.
    duration = now_ + (UINT32_MAX - current_->start_at);
  } else {
    duration = now_ - current_->start_at;
  }

  return {current_->track, duration};
}

auto StreamCues::hasStream() -> bool {
  // 'current_' might be tracking how long we've been playing nothing for.
  return (current_ && current_->track) || !upcoming_.empty();
}

}  // namespace audio
