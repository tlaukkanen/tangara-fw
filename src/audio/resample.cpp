#include "resample.hpp"

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

static constexpr char kTag[] = "resample";

static constexpr double kLowPassRatio = 0.5;
static constexpr size_t kNumFilters = 64;
static constexpr size_t kTapsPerFilter = 16;

typedef std::array<float, kTapsPerFilter> Filter;
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
  channel_buffer_size_ = kTapsPerFilter * 16;

  for (int i = 0; i < num_channels; i++) {
    channel_buffers_[i] =
        static_cast<float*>(calloc(sizeof(float), channel_buffer_size_));
  }

  output_offset_ = kTapsPerFilter / 2.0f;
  input_index_ = kTapsPerFilter;

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

  int half_taps = kTapsPerFilter / 2;
  while (output_frames > 0) {
    if (output_offset_ >= input_index_ - half_taps) {
      if (input_frames > 0) {
        // Check whether the channel buffers will overflow with the addition of
        // this sample. If so, we need to move the remaining contents back to
        // the beginning of the buffer.
        if (input_index_ == channel_buffer_size_) {
          for (int i = 0; i < num_channels_; ++i) {
            memmove(channel_buffers_[i],
                    channel_buffers_[i] + channel_buffer_size_ - kTapsPerFilter,
                    kTapsPerFilter * sizeof(float));
          }

          output_offset_ -= channel_buffer_size_ - kTapsPerFilter;
          input_index_ -= channel_buffer_size_ - kTapsPerFilter;
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

      output_offset_ += (1.0f / factor_);
      output_frames--;
    }
  }

  return {samples_used, samples_produced};
}

auto InitFilter(int index) -> void {
  const double a0 = 0.35875;
  const double a1 = 0.48829;
  const double a2 = 0.14128;
  const double a3 = 0.01168;

  double fraction =
      static_cast<double>(index) / static_cast<double>(kNumFilters);
  double filter_sum = 0.0;

  // "dist" is the absolute distance from the sinc maximum to the filter tap to
  // be calculated, in radians "ratio" is that distance divided by half the tap
  // count such that it reaches π at the window extremes

  // Note that with this scaling, the odd terms of the Blackman-Harris
  // calculation appear to be negated with respect to the reference formula
  // version.

  Filter& filter = sFilters[index];
  std::array<double, kTapsPerFilter> working_buffer{};
  for (int i = 0; i < kTapsPerFilter; ++i) {
    double dist = fabs((kTapsPerFilter / 2.0 - 1.0) + fraction - i) * M_PI;
    double ratio = dist / (kTapsPerFilter / 2.0);
    double value;

    if (dist != 0.0) {
      value = sin(dist * kLowPassRatio) / (dist * kLowPassRatio);

      // Blackman-Harris window
      value *= a0 + a1 * cos(ratio) + a2 * cos(2 * ratio) + a3 * cos(3 * ratio);
    } else {
      value = 1.0;
    }

    working_buffer[i] = value;
    filter_sum += value;
  }

  // filter should have unity DC gain

  double scaler = 1.0 / filter_sum;
  double error = 0.0;

  for (int i = kTapsPerFilter / 2; i < kTapsPerFilter;
       i = kTapsPerFilter - i - (i >= kTapsPerFilter / 2)) {
    working_buffer[i] *= scaler;
    filter[i] = working_buffer[i] - error;
    error += static_cast<double>(filter[i]) - working_buffer[i];
  }
}

auto Resampler::Subsample(int channel) -> float {
  float sum1, sum2;

  cpp::span<float> source{channel_buffers_[channel], channel_buffer_size_};

  int offset_integral = std::floor(output_offset_);
  source = source.subspan(offset_integral);
  float offset_fractional = output_offset_ - offset_integral;

  /*
// no interpolate
size_t filter_index = std::floor(offset_fractional * kNumFilters + 0.5f);
//ESP_LOGI(kTag, "selected filter %u of %u", filter_index, kNumFilters);
int start_offset = kTapsPerFilter / 2 + 1;
//ESP_LOGI(kTag, "using offset of %i, length %u", start_offset, kTapsPerFilter);

return ApplyFilter(
    sFilters[filter_index],
    {source.data() - start_offset, kTapsPerFilter});
  */

  offset_fractional *= kNumFilters;
  int filter_index = std::floor(offset_fractional);

  sum1 = ApplyFilter(sFilters[filter_index],
                     {source.data() - kTapsPerFilter / 2 + 1, kTapsPerFilter});

  offset_fractional -= filter_index;

  sum2 = ApplyFilter(sFilters[filter_index + 1],
                     {source.data() - kTapsPerFilter / 2 + 1, kTapsPerFilter});

  return (sum2 * offset_fractional) + (sum1 * (1.0f - offset_fractional));
}

auto Resampler::ApplyFilter(cpp::span<float> filter, cpp::span<float> input)
    -> float {
  float sum = 0.0;
  for (int i = 0; i < kTapsPerFilter; i++) {
    sum += filter[i] * input[i];
  }
  return sum;
}

}  // namespace audio
