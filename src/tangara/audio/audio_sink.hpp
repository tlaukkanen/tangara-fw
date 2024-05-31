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

namespace audio {

/*
 * Interface for classes that use PCM samples to create noises for the user.
 *
 * These classes do not generally have any specific task for their work, and
 * simply help to mediate working out the correct PCM format, and then sending
 * those samples to the appropriate hardware driver.
 */
class IAudioOutput {
 public:
  IAudioOutput() : mode_(Modes::kOff) {}

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
  auto mode(Modes m) -> void {
    if (mode_ == m) {
      return;
    }
    changeMode(m);
    mode_ = m;
  }
  auto mode() -> Modes { return mode_; }

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

 protected:
  Modes mode_;

  virtual auto changeMode(Modes new_mode) -> void = 0;
};

}  // namespace audio
