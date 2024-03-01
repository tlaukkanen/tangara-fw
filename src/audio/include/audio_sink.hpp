/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "idf_additions.h"

namespace audio {

/*
 * Interface for classes that use PCM samples to create noises for the user.
 *
 * These classes do not generally have any specific task for their work, and
 * simply help to mediate working out the correct PCM format, and then sending
 * those samples to the appropriate hardware driver.
 */
class IAudioOutput {
 private:
  StreamBufferHandle_t stream_;

 public:
  IAudioOutput(StreamBufferHandle_t stream) : stream_(stream) {}

  virtual ~IAudioOutput() {}

  enum class Modes {
    kOff,
    kOnPaused,
    kOnPlaying,
  };

  /*
   * Indicates whether this output is currently being sent samples. If this is
   * false, the output should place itself into a low power state.
   */
  virtual auto SetMode(Modes) -> void = 0;

  virtual auto SetVolumeImbalance(int_fast8_t balance) -> void = 0;

  virtual auto SetVolume(uint16_t) -> void = 0;

  virtual auto GetVolume() -> uint16_t = 0;

  virtual auto GetVolumePct() -> uint_fast8_t = 0;
  virtual auto GetVolumeDb() -> int_fast16_t = 0;

  virtual auto SetVolumePct(uint_fast8_t) -> bool = 0;
  virtual auto SetVolumeDb(int_fast16_t) -> bool = 0;

  virtual auto AdjustVolumeUp() -> bool = 0;
  virtual auto AdjustVolumeDown() -> bool = 0;

  struct Format {
    uint32_t sample_rate;
    uint_fast8_t num_channels;
    uint_fast8_t bits_per_sample;

    bool operator==(const Format&) const = default;
  };

  virtual auto PrepareFormat(const Format&) -> Format = 0;
  virtual auto Configure(const Format& format) -> void = 0;

  auto stream() -> StreamBufferHandle_t { return stream_; }
};

}  // namespace audio
