/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio_task.hpp"
#include "driver_cache.hpp"
#include "esp_err.h"
#include "fatfs_audio_input.hpp"
#include "i2s_audio_output.hpp"
#include "result.hpp"
#include "span.hpp"

#include "audio_element.hpp"
#include "gpio_expander.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"

namespace audio {

/*
 * Creates and links together audio elements into a pipeline. This is the main
 * entrypoint to playing audio on the system.
 */
class AudioPlayback {
 public:
  explicit AudioPlayback(drivers::DriverCache* drivers);
  ~AudioPlayback();

  /*
   * Begins playing the file at the given FatFS path. This will interrupt any
   * currently in-progress playback.
   */
  auto Play(const std::string& filename) -> void;

  auto LogStatus() -> void;

  // Not copyable or movable.
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;

 private:
  std::unique_ptr<FatfsAudioInput> file_source_;
  std::unique_ptr<I2SAudioOutput> i2s_output_;
  std::vector<std::unique_ptr<IAudioElement>> elements_;
};

}  // namespace audio
