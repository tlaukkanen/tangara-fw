#pragma once

#include <sys/_stdint.h>
#include <vector>

#include "span.hpp"

#include "sample.hpp"

namespace audio {

class Resampler {
 public:
  Resampler(uint32_t source_sample_rate,
            uint32_t target_sample_rate,
            uint8_t num_channels);

  ~Resampler();

  auto source_sample_rate() -> uint32_t { return source_sample_rate_; }
  auto target_sample_rate() -> uint32_t { return target_sample_rate_; }
  auto channels() -> uint_fast8_t { return num_channels_; }

  auto Process(cpp::span<const sample::Sample> input,
               cpp::span<sample::Sample> output,
               bool end_of_data) -> std::pair<size_t,size_t>;

 private:
  auto Subsample(int channel) -> float;
  auto ApplyFilter(cpp::span<float> filter, cpp::span<float> input) -> float;

  uint32_t source_sample_rate_;
  uint32_t target_sample_rate_;
  float factor_;
  uint8_t num_channels_;

  std::vector<float*> channel_buffers_;
  size_t channel_buffer_size_;

  float output_offset_;
  int32_t input_index_;
};

}  // namespace audio