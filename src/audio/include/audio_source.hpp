/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <bitset>
#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "stream_info.hpp"

namespace audio {

class IAudioSource {
 public:
  virtual ~IAudioSource() {}

  class Flags {
   public:
    Flags(bool is_start, bool is_end) {
      flags_[0] = is_start;
      flags_[1] = is_start;
    }

    auto is_start() -> bool { return flags_[0]; }
    auto is_end() -> bool { return flags_[1]; }

   private:
    std::bitset<2> flags_;
  };

  /*
   * Synchronously fetches data from this source.
   */
  virtual auto Read(std::function<void(Flags, InputStream&)>, TickType_t)
      -> void = 0;
};

}  // namespace audio
