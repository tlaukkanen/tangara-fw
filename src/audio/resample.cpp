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
#include "stream_info.hpp"

namespace audio {

static constexpr double kLowPassRatio = 0.5;
static constexpr size_t kNumFilters = 64;
static constexpr size_t kFilterSize = 16;

typedef std::array<float, kFilterSize> Filter;
static std::array<Filter, kNumFilters + 1> sFilters{};
static bool sFiltersInitialised = false;

auto InitFilter(int index) -> void;

Resampler::Resampler(uint32_t source_sample_rate,
                     uint32_t target_sample_rate,
                     uint8_t num_channels)
    : source_sample_rate_(source_sample_rate),
      target_sample_rate_(target_sample_rate),
      factor_(static_cast<double>(target_sample_rate) /
              static_cast<double>(source_sample_rate)),
      num_channels_(num_channels) {
  channel_buffers_.resize(num_channels);
  channel_buffer_size_ = kFilterSize * 16;

  for (int i = 0; i < num_channels; i++) {
    channel_buffers_[i] =
        static_cast<float*>(calloc(sizeof(float), channel_buffer_size_));
  }

  output_offset_ = kFilterSize / 2.0f;
  input_index_ = kFilterSize;

  if (!sFiltersInitialised) {
    sFiltersInitialised = true;
    for (int i = 0; i < kNumFilters + 1; i++) {
      InitFilter(i);
    }
  }
}

Resampler::~Resampler() {}

auto Resampler::Process(cpp::span<const sample::Sample> input,
                        cpp::span<sample::Sample> output,
                        bool end_of_data) -> std::pair<size_t, size_t> {
  size_t samples_used = 0;
  size_t samples_produced = 0;

  size_t input_frames = input.size() / num_channels_;
  size_t output_frames = output.size() / num_channels_;

  int half_taps = kFilterSize / 2;
  while (output_frames > 0) {
    if (output_offset_ >= input_index_ - half_taps) {
      if (input_frames > 0) {
        // Check whether the channel buffers will overflow with the addition of
        // this sample. If so, we need to move the remaining contents back to
        // the beginning of the buffer.
        if (input_index_ == channel_buffer_size_) {
          for (int i = 0; i < num_channels_; ++i) {
            memmove(channel_buffers_[i],
                    channel_buffers_[i] + channel_buffer_size_ - kFilterSize,
                    kFilterSize * sizeof(float));
          }

          output_offset_ -= channel_buffer_size_ - kFilterSize;
          input_index_ -= channel_buffer_size_ - kFilterSize;
        }

        for (int i = 0; i < num_channels_; ++i) {
          channel_buffers_[i][input_index_] =
              sample::ToFloat(input[samples_used++]);
        }

        input_index_++;
        input_frames--;
      } else {
        break;
      }
    } else {
      for (int i = 0; i < num_channels_; i++) {
        output[samples_produced++] = sample::FromFloat(Subsample(i));
      }

      // NOTE: floating point division here is potentially slow due to FPU
      // limitations. Consider explicitly bunding the xtensa libgcc divsion via
      // reciprocal implementation if we care about portability between
      // compilers.
      output_offset_ += 1.0f / factor_;
      output_frames--;
    }
  }

  return {samples_used, samples_produced};
}

/*
 * Constructs the filter in-place for the given index of sFilters. This only
 * needs to be done once, per-filter. 64-bit math is okay here, because filters
 * will not be initialised within a performance critical path.
 */
auto InitFilter(int index) -> void {
  Filter& filter = sFilters[index];
  std::array<double, kFilterSize> working_buffer{};

  double fraction = index / static_cast<double>(kNumFilters);
  double filter_sum = 0.0;

  for (int i = 0; i < kFilterSize; ++i) {
    // "dist" is the absolute distance from the sinc maximum to the filter tap
    //  to be calculated, in radians.
    double dist = fabs((kFilterSize / 2.0 - 1.0) + fraction - i) * M_PI;
    // "ratio" is that distance divided by half the tap count such that it
    // reaches π at the window extremes
    double ratio = dist / (kFilterSize / 2.0);

    double value;
    if (dist != 0.0) {
      value = sin(dist * kLowPassRatio) / (dist * kLowPassRatio);

      // Hann window. We could alternatively use a Blackman Harris window,
      // however our unusually small filter size makes the Hann window's
      // steeper cutoff more important.
      value *= 0.5 * (1.0 + cos(ratio));
    } else {
      value = 1.0;
    }

    working_buffer[i] = value;
    filter_sum += value;
  }

  // Filter should have unity DC gain
  double scaler = 1.0 / filter_sum;
  double error = 0.0;

  for (int i = kFilterSize / 2; i < kFilterSize;
       i = kFilterSize - i - (i >= kFilterSize / 2)) {
    working_buffer[i] *= scaler;
    filter[i] = working_buffer[i] - error;
    error += static_cast<double>(filter[i]) - working_buffer[i];
  }
}

/*
 * Performs sub-sampling with interpolation for the given channel. Assumes that
 * the channel buffer has already been filled with samples.
 */
auto Resampler::Subsample(int channel) -> float {
  cpp::span<float> source{channel_buffers_[channel], channel_buffer_size_};

  int offset_integral = std::floor(output_offset_);
  source = source.subspan(offset_integral);
  float offset_fractional = output_offset_ - offset_integral;

  offset_fractional *= kNumFilters;
  int filter_index = std::floor(offset_fractional);

  float sum1 = ApplyFilter(sFilters[filter_index],
                           {source.data() - kFilterSize / 2 + 1, kFilterSize});

  offset_fractional -= filter_index;

  float sum2 = ApplyFilter(sFilters[filter_index + 1],
                           {source.data() - kFilterSize / 2 + 1, kFilterSize});

  return (sum2 * offset_fractional) + (sum1 * (1.0f - offset_fractional));
}

auto Resampler::ApplyFilter(cpp::span<float> filter, cpp::span<float> input)
    -> float {
  float sum = 0.0;
  for (int i = 0; i < kFilterSize; i++) {
    sum += filter[i] * input[i];
  }
  return sum;
}

}  // namespace audio
