/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
#include <cstdint>
#include <memory>

#include "resample.hpp"
#include "sample.hpp"
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

  auto Resample(InputStream&, OutputStream&) -> bool;
  auto ApplyDither(cpp::span<sample::Sample> samples, uint_fast8_t bits) -> void;
  auto Downscale(cpp::span<sample::Sample>, cpp::span<int16_t>) -> void;
 
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

  std::unique_ptr<Resampler> resampler_;

  std::unique_ptr<RawStream> input_stream_;
  std::unique_ptr<RawStream> resampled_stream_;

  StreamInfo::Pcm target_format_;
  StreamBufferHandle_t source_;
  StreamBufferHandle_t sink_;
};

}  // namespace audio
