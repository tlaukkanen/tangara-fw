/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "stream_info.hpp"

namespace audio {

class IAudioSource {
 public:
  virtual ~IAudioSource() {}

  /*
   * Synchronously fetches data from this source.
   */
  virtual auto Read(std::function<bool(StreamInfo::Format)>,
                    std::function<size_t(cpp::span<const std::byte>)>,
                    TickType_t) -> void = 0;
};

}  // namespace audio
