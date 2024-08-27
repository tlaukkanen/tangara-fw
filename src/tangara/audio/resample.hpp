/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <span>
#include <vector>

#include "speex/speex_resampler.h"

#include "sample.hpp"

namespace audio {

class Resampler {
 public:
  Resampler(uint32_t source_sample_rate,
            uint32_t target_sample_rate,
            uint8_t num_channels);

  ~Resampler();

  auto sourceRate() -> uint32_t;

  auto Process(std::span<sample::Sample> input,
               std::span<sample::Sample> output,
               bool end_of_data) -> std::pair<size_t, size_t>;

 private:
  int err_;
  SpeexResamplerState* resampler_;
  uint8_t num_channels_;
};

}  // namespace audio
