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

#include "codec.hpp"
#include "stream_info.hpp"
#include "track.hpp"
#include "types.hpp"

namespace audio {

class IAudioSource {
 public:
  virtual ~IAudioSource() {}

  virtual auto HasNewStream() -> bool = 0;
  virtual auto NextStream() -> std::shared_ptr<codecs::IStream> = 0;
};

}  // namespace audio
