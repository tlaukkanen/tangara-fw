/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
namespace drivers {

class IAudioBackend {
 public:
  virtual ~IAudioBackend() {}

  enum SampleRate {};
  enum BitDepth {};

  virtual auto Configure(SampleRate sample_rate, BitDepth bit_depth)
      -> bool = 0;
  virtual auto WritePcmData(uint8_t* data, size_t length) -> bool = 0;

  virtual auto SetVolume(uint8_t percent) -> void = 0;
};

}  // namespace drivers
