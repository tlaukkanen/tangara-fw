#pragma once

#include <sys/_stdint.h>
#include <vector>

#include "span.hpp"
#include "speex/speex_resampler.h"

#include "sample.hpp"

namespace audio {

class Resampler {
 public:
  Resampler(uint32_t source_sample_rate,
            uint32_t target_sample_rate,
            uint8_t num_channels);

  ~Resampler();

  auto Process(cpp::span<sample::Sample> input,
               cpp::span<sample::Sample> output,
               bool end_of_data) -> std::pair<size_t, size_t>;

 private:
  int err_;
  SpeexResamplerState* resampler_;
  uint8_t num_channels_;
};

}  // namespace audio