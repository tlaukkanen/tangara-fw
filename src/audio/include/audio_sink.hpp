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

class IAudioSink {
 private:
  StreamBufferHandle_t stream_;

 public:
  IAudioSink(size_t buffer_size, uint32_t caps)
      : stream_(xStreamBufferCreateWithCaps(buffer_size, 1, caps)) {}

  virtual ~IAudioSink() { vStreamBufferDeleteWithCaps(stream_); }

  virtual auto SetInUse(bool) -> void {}

  virtual auto SetVolumeImbalance(int_fast8_t balance) -> void = 0;
  virtual auto SetVolume(uint_fast8_t percent) -> void = 0;
  virtual auto GetVolume() -> uint_fast8_t = 0;
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
