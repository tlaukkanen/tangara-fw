/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_element.hpp"

namespace audio {

class AudioOutput : IAudioElement {
 public:
  AudioOutput();
  ~AudioOutput();

  auto GetAudioElement() -> audio_element_handle_t { return element_; }

  auto SetInputBuffer(StreamBufferHandle_t buffer) -> void;

  virtual auto Configure(audio_element_info_t& info) -> void = 0;
  virtual auto SetSoftMute(bool enabled) -> void = 0;

 private:
  audio_element_handle_t element_;
  uint8_t volume_;
};

}  // namespace audio
