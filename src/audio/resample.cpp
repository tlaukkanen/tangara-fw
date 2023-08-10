/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "resample.hpp"
/*
 * This file contains the implementation for a 32-bit floating point resampler.
 * It is largely based on David Bryant's ART resampler, which is BSD-licensed,
 * and available at https://github.com/dbry/audio-resampler/.
 *
 * This resampler uses windowed sinc interpolation filters, with an additional
 * lowpass filter to reduce aliasing.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <numeric>

#include "esp_log.h"

#include "sample.hpp"
#include "speex/speex_resampler.h"
#include "stream_info.hpp"

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

auto Resampler::Process(cpp::span<sample::Sample> input,
                        cpp::span<sample::Sample> output,
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
