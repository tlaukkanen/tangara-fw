/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
#include <cstdint>
#include <memory>

#include "samplerate.h"

#include "audio_decoder.hpp"
#include "audio_sink.hpp"
#include "audio_source.hpp"
#include "codec.hpp"
#include "pipeline.hpp"
#include "stream_info.hpp"

namespace audio {

/*
 * Handles the final downmix + resample + quantisation stage of audio,
 * generation sending the result directly to an IAudioSink.
 */
class SinkMixer {
 public:
  SinkMixer(StreamBufferHandle_t dest);
  ~SinkMixer();

  auto MixAndSend(InputStream&, const StreamInfo::Pcm&) -> std::size_t;

 private:
  auto Main() -> void;

  auto SetTargetFormat(const StreamInfo::Pcm& format) -> void;
  auto HandleBytes() -> void;

  template <typename T>
  auto ConvertFixedToFloating(InputStream&, OutputStream&) -> void;
  auto Resample(float, int, InputStream&, OutputStream&) -> void;
  template <typename T>
  auto Quantise(InputStream&) -> std::size_t;

  enum class Command {
    kReadBytes,
    kSetSourceFormat,
    kSetTargetFormat,
  };

  struct Args {
    Command cmd;
    StreamInfo::Pcm format;
  };

  QueueHandle_t commands_;
  SemaphoreHandle_t is_idle_;

  SRC_STATE* resampler_;

  std::unique_ptr<RawStream> input_stream_;
  std::unique_ptr<RawStream> floating_point_stream_;
  std::unique_ptr<RawStream> resampled_stream_;

  cpp::span<std::byte> quantisation_buffer_;
  cpp::span<short> quantisation_buffer_as_shorts_;
  cpp::span<int> quantisation_buffer_as_ints_;

  StreamInfo::Pcm target_format_;
  StreamBufferHandle_t source_;
  StreamBufferHandle_t sink_;
};

template <>
auto SinkMixer::ConvertFixedToFloating<short>(InputStream&, OutputStream&)
    -> void;
template <>
auto SinkMixer::ConvertFixedToFloating<int>(InputStream&, OutputStream&)
    -> void;

template <>
auto SinkMixer::Quantise<short>(InputStream&) -> std::size_t;
template <>
auto SinkMixer::Quantise<int>(InputStream&) -> std::size_t;

}  // namespace audio
