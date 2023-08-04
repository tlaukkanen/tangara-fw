#include "resample.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <numeric>

#include "esp_log.h"

#include "sample.hpp"
#include "stream_info.hpp"

namespace audio {

static constexpr size_t kFilterSize = 1536;

constexpr auto calc_deltas(const std::array<int32_t, kFilterSize>& filter)
    -> std::array<int32_t, kFilterSize> {
  std::array<int32_t, kFilterSize> deltas;
  for (size_t n = 0; n < kFilterSize - 1; n++)
    deltas[n] = filter[n + 1] - filter[n];
  return deltas;
}

static const std::array<int32_t, kFilterSize> kFilter{
#include "fir.h"
};

static const std::array<int32_t, kFilterSize> kFilterDeltas =
    calc_deltas(kFilter);

class Channel {
 public:
  Channel(uint32_t src_rate,
          uint32_t dest_rate,
          size_t chunk_size,
          size_t skip);
  ~Channel();

  auto output_chunk_size() -> size_t { return output_chunk_size_; }

  auto FlushSamples(cpp::span<sample::Sample> out) -> size_t;
  auto AddSample(sample::Sample, cpp::span<sample::Sample> out) -> std::size_t;
  auto ApplyFilter() -> sample::Sample;

 private:
  size_t output_chunk_size_;
  size_t skip_;

  uint32_t factor_; /* factor */

  uint32_t time_; /* time */

  uint32_t time_per_filter_iteration_; /* output step */
  uint32_t filter_step_;               /* filter step */
  uint32_t filter_end_;                /* filter end */

  int32_t unity_scale_; /* unity scale */

  int32_t samples_per_filter_wing_;  /* extra samples */
  int32_t latest_sample_;            /* buffer index */
  cpp::span<int32_t> sample_buffer_; /* the buffer */
};

enum {
  Nl = 8,               /* 2^Nl samples per zero crossing in fir */
  Nη = 8,               /* phase bits for filter interpolation */
  kPhaseBits = Nl + Nη, /* phase bits (fract of fixed point) */
  One = 1 << kPhaseBits,
};

Channel::Channel(uint32_t irate, uint32_t orate, size_t count, size_t skip)
    : skip_(skip) {
  factor_ = ((uint64_t)orate << kPhaseBits) / irate;
  if (factor_ != One) {
    time_per_filter_iteration_ = ((uint64_t)irate << kPhaseBits) / orate;
    filter_step_ = 1 << (Nl + Nη);
    filter_end_ = kFilterSize << Nη;
    samples_per_filter_wing_ = 1 + (filter_end_ / filter_step_);
    unity_scale_ = 13128; /* unity scale factor for fir */
    if (factor_ < One) {
      unity_scale_ *= factor_;
      unity_scale_ >>= kPhaseBits;
      filter_step_ *= factor_;
      filter_step_ >>= kPhaseBits;
      samples_per_filter_wing_ *= time_per_filter_iteration_;
      samples_per_filter_wing_ >>= kPhaseBits;
    }
    latest_sample_ = samples_per_filter_wing_;
    time_ = latest_sample_ << kPhaseBits;

    size_t buf_size = samples_per_filter_wing_ * 2 + count;
    int32_t* buf = new int32_t[buf_size];
    sample_buffer_ = {buf, buf_size};
    count += buf_size; /* account for buffer accumulation */
  }
  output_chunk_size_ = ((uint64_t)count * factor_) >> kPhaseBits;
}

Channel::~Channel() {
  delete sample_buffer_.data();
}

auto Channel::ApplyFilter() -> sample::Sample {
  uint32_t iteration, p, i;
  int32_t *sample, a;

  int64_t value = 0;

  // I did my best, but I'll be honest with you I've no idea about any of this
  // maths stuff.

  // Left wing of the filter.
  sample = &sample_buffer_[time_ >> kPhaseBits];
  p = time_ & ((1 << kPhaseBits) - 1);
  iteration = factor_ < One ? (factor_ * p) >> kPhaseBits : p;
  while (iteration < filter_end_) {
    i = iteration >> Nη;
    a = iteration & ((1 << Nη) - 1);
    iteration += filter_step_;
    a *= kFilterDeltas[i];
    a >>= Nη;
    a += kFilter[i];
    value += static_cast<int64_t>(*--sample) * a;
  }

  // Right wing of the filter.
  sample = &sample_buffer_[time_ >> kPhaseBits];
  p = (One - p) & ((1 << kPhaseBits) - 1);
  iteration = factor_ < One ? (factor_ * p) >> kPhaseBits : p;
  if (p == 0) /* skip h[0] as it was already been summed above if p == 0 */
    iteration += filter_step_;
  while (iteration < filter_end_) {
    i = iteration >> Nη;
    a = iteration & ((1 << Nη) - 1);
    iteration += filter_step_;
    a *= kFilterDeltas[i];
    a >>= Nη;
    a += kFilter[i];
    value += static_cast<int64_t>(*sample++) * a;
  }

  /* scale */
  value >>= 2;
  value *= unity_scale_;
  value >>= 27;

  return sample::Clip(value);
}

auto Channel::FlushSamples(cpp::span<sample::Sample> out) -> size_t {
  size_t zeroes_needed = (2 * samples_per_filter_wing_) - latest_sample_;
  size_t produced = 0;
  while (zeroes_needed > 0) {
    produced += AddSample(0, out.subspan(produced));
    zeroes_needed--;
  }
  return produced;
}

auto Channel::AddSample(sample::Sample in, cpp::span<sample::Sample> out)
    -> size_t {
  // Add the latest sample to our working buffer.
  sample_buffer_[latest_sample_++] = in;

  // If we don't have enough samples to run the filter, then bail out and wait
  // for more.
  if (latest_sample_ < 2 * samples_per_filter_wing_) {
    return 0;
  }

  // Apply the filter to the buffered samples. First, we work out how long (in
  // samples) we can run the filter for before running out. This isn't as
  // trivial as it might look; e.g. depending on the resampling factor we might
  // be doubling the number of samples, or halving them.
  uint32_t max_time = (latest_sample_ - samples_per_filter_wing_) << kPhaseBits;
  size_t samples_output = 0;
  while (time_ < max_time) {
    out[skip_ * samples_output++] = ApplyFilter();
    time_ += time_per_filter_iteration_;
  }

  // If we are approaching the end of our buffer, we need to shift all the data
  // in it down to the front to make room for more samples.
  int32_t current_sample = time_ >> kPhaseBits;
  if (current_sample >= (sample_buffer_.size() - samples_per_filter_wing_)) {
    // NB: bit shifting back and forth means we're only modifying `time` by
    // whole samples.
    time_ -= current_sample << kPhaseBits;
    time_ += samples_per_filter_wing_ << kPhaseBits;

    int32_t new_current_sample = time_ >> kPhaseBits;
    new_current_sample -= samples_per_filter_wing_;
    current_sample -= samples_per_filter_wing_;

    int32_t samples_to_move = latest_sample_ - current_sample;
    if (samples_to_move > 0) {
      auto samples = sample_buffer_.subspan(current_sample, samples_to_move);
      std::copy_backward(samples.begin(), samples.end(),
                         sample_buffer_.first(new_current_sample).end());
      latest_sample_ = new_current_sample + samples_to_move;
    } else {
      latest_sample_ = new_current_sample;
    }
  }

  return samples_output;
}

static const size_t kChunkSizeSamples = 256;

Resampler::Resampler(uint32_t source_sample_rate,
                     uint32_t target_sample_rate,
                     uint8_t num_channels)
    : source_sample_rate_(source_sample_rate),
      target_sample_rate_(target_sample_rate),
      factor_(((uint64_t)target_sample_rate << kPhaseBits) /
              source_sample_rate),
      num_channels_(num_channels),
      channels_() {
  for (int i = 0; i < num_channels; i++) {
    channels_.emplace_back(source_sample_rate, target_sample_rate,
                           kChunkSizeSamples, num_channels);
  }
}

Resampler::~Resampler() {}

auto Resampler::Process(cpp::span<const sample::Sample> input,
                        cpp::span<sample::Sample> output,
                        bool end_of_data) -> std::pair<size_t, size_t> {
  size_t samples_used = 0;
  std::vector<size_t> samples_produced = {num_channels_, 0};
  size_t total_samples_produced = 0;

  size_t slop = (factor_ >> kPhaseBits) + 1;

  uint_fast8_t cur_channel = 0;

  while (input.size() > samples_used &&
         output.size() > total_samples_produced + slop) {
    // Work out where the next set of samples should be placed.
    size_t next_output_index =
        (samples_produced[cur_channel] * num_channels_) + cur_channel;

    // Generate the next samples
    size_t new_samples = channels_[cur_channel].AddSample(
        input[samples_used++], output.subspan(next_output_index));

    samples_produced[cur_channel] += new_samples;
    total_samples_produced += new_samples;

    cur_channel = (cur_channel + 1) % num_channels_;
  }

  return {samples_used, total_samples_produced};
}

}  // namespace audio
