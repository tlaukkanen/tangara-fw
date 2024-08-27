/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "audio/resample.hpp"
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <numeric>

#include "esp_log.h"
#include "speex/speex_resampler.h"

#include "sample.hpp"

namespace audio {

static constexpr int kQuality = SPEEX_RESAMPLER_QUALITY_MIN;

Resampler::Resampler(uint32_t source_sample_rate,
                     uint32_t target_sample_rate,
                     uint8_t num_channels)
    : err_(0),
      resampler_(speex_resampler_init(num_channels,
                                      source_sample_rate,
                                      target_sample_rate,
                                      kQuality,
                                      &err_)),
      num_channels_(num_channels) {
  speex_resampler_skip_zeros(resampler_);
  assert(err_ == 0);
}

Resampler::~Resampler() {
  speex_resampler_destroy(resampler_);
}

auto Resampler::sourceRate() -> uint32_t {
  uint32_t input = 0;
  uint32_t output = 0;
  speex_resampler_get_rate(resampler_, &input, &output);
  return input;
}

auto Resampler::Process(std::span<sample::Sample> input,
                        std::span<sample::Sample> output,
                        bool end_of_data) -> std::pair<size_t, size_t> {
  uint32_t frames_used = input.size() / num_channels_;
  uint32_t frames_produced = output.size() / num_channels_;

  int err = speex_resampler_process_interleaved_int(
      resampler_, input.data(), &frames_used, output.data(), &frames_produced);
  assert(err == 0);

  return {frames_used * num_channels_, frames_produced * num_channels_};
}

}  // namespace audio
