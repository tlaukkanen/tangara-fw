/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <memory>

#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "audio/audio_source.hpp"
#include "audio/resample.hpp"
#include "codec.hpp"
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

  auto beginStream(std::shared_ptr<TrackInfo>) -> void;
  auto continueStream(std::span<sample::Sample>) -> void;
  auto endStream() -> void;

 private:
  auto Main() -> void;

  auto handleBeginStream(std::shared_ptr<TrackInfo>) -> void;
  auto handleContinueStream(size_t samples_available) -> void;
  auto handleEndStream() -> void;

  auto handleSamples(std::span<sample::Sample>) -> size_t;

  auto sendToSink(std::span<sample::Sample>) -> void;

  struct Args {
    std::shared_ptr<TrackInfo>* track;
    size_t samples_available;
    bool is_end_of_stream;
  };
  QueueHandle_t commands_;

  std::unique_ptr<Resampler> resampler_;

  StreamBufferHandle_t source_;
  std::span<sample::Sample> input_buffer_;
  std::span<std::byte> input_buffer_as_bytes_;

  std::span<sample::Sample> resampled_buffer_;

  std::shared_ptr<IAudioOutput> sink_;
  IAudioOutput::Format source_format_;
  IAudioOutput::Format target_format_;
  size_t leftover_bytes_;

  uint32_t samples_sunk_;
};

}  // namespace audio
