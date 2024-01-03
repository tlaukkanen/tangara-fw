/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>

#include "audio_sink.hpp"
#include "audio_source.hpp"
#include "codec.hpp"
#include "resample.hpp"
#include "sample.hpp"

namespace audio {

/*
 * Handle to a persistent task that converts samples between formats (sample
 * rate, channels, bits per sample), in order to put samples in the preferred
 * format of the current output device. The resulting samples are forwarded
 * to the output device's sink stream.
 */
class SampleConverter {
 public:
  SampleConverter();
  ~SampleConverter();

  auto SetOutput(std::shared_ptr<IAudioOutput>) -> void;

  auto ConvertSamples(cpp::span<sample::Sample>,
                      const IAudioOutput::Format& format,
                      bool is_eos) -> void;

 private:
  auto Main() -> void;

  auto SetTargetFormat(const IAudioOutput::Format& format) -> void;
  auto HandleSamples(cpp::span<sample::Sample>, bool) -> size_t;

  struct Args {
    IAudioOutput::Format format;
    size_t samples_available;
    bool is_end_of_stream;
  };
  QueueHandle_t commands_;

  std::unique_ptr<Resampler> resampler_;

  StreamBufferHandle_t source_;
  cpp::span<sample::Sample> input_buffer_;
  cpp::span<std::byte> input_buffer_as_bytes_;

  cpp::span<sample::Sample> resampled_buffer_;

  std::shared_ptr<IAudioOutput> sink_;
  IAudioOutput::Format source_format_;
  IAudioOutput::Format target_format_;
  size_t leftover_bytes_;
};

}  // namespace audio
