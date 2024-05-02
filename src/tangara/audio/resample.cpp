/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "audio/resample.hpp"

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
  assert(err_ == 0);
}

Resampler::~Resampler() {
  speex_resampler_destroy(resampler_);
}

auto Resampler::Process(std::span<sample::Sample> input,
                        std::span<sample::Sample> output,
                        bool end_of_data) -> std::pair<size_t, size_t> {
  uint32_t samples_used = input.size() / num_channels_;
  uint32_t samples_produced = output.size() / num_channels_;

  int err = speex_resampler_process_interleaved_int(
      resampler_, input.data(), &samples_used, output.data(),
      &samples_produced);
  assert(err == 0);

  return {samples_used * num_channels_, samples_produced * num_channels_};
}

}  // namespace audio
