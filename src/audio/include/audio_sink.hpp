/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include "audio_element.hpp"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "idf_additions.h"
#include "stream_info.hpp"

namespace audio {

class IAudioSink {
 private:
  // TODO: tune. at least about 12KiB seems right for mp3
  static const std::size_t kDrainBufferSize = 24 * 1024;
  StreamBufferHandle_t stream_;

 public:
  IAudioSink()
      : stream_(xStreamBufferCreateWithCaps(
            kDrainBufferSize,
            1,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) {}

  virtual ~IAudioSink() { vStreamBufferDeleteWithCaps(stream_); }

  virtual auto SetInUse(bool) -> void {}

  virtual auto SetVolumeImbalance(int_fast8_t balance) -> void = 0;
  virtual auto SetVolume(uint_fast8_t percent) -> void = 0;
  virtual auto GetVolume() -> uint_fast8_t = 0;
  virtual auto AdjustVolumeUp() -> bool = 0;
  virtual auto AdjustVolumeDown() -> bool = 0;

  virtual auto PrepareFormat(const StreamInfo::Pcm&) -> StreamInfo::Pcm = 0;
  virtual auto Configure(const StreamInfo::Pcm& format) -> void = 0;

  auto stream() -> StreamBufferHandle_t { return stream_; }
};

}  // namespace audio
