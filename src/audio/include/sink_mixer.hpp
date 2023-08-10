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
  SinkMixer(IAudioSink* sink);
  ~SinkMixer();

  auto MixAndSend(cpp::span<sample::Sample>,
                  const IAudioSink::Format& format,
                  bool is_eos) -> void;

 private:
  auto Main() -> void;

  auto SetTargetFormat(const StreamInfo::Pcm& format) -> void;
  auto HandleSamples(cpp::span<sample::Sample>, bool) -> size_t;

  auto ApplyDither(cpp::span<sample::Sample> samples, uint_fast8_t bits)
      -> void;

  struct Args {
    IAudioSink::Format format;
    size_t samples_available;
    bool is_end_of_stream;
  };
  QueueHandle_t commands_;

  std::unique_ptr<Resampler> resampler_;

  StreamBufferHandle_t source_;
  cpp::span<sample::Sample> input_buffer_;
  cpp::span<std::byte> input_buffer_as_bytes_;

  cpp::span<sample::Sample> resampled_buffer_;

  IAudioSink* sink_;
  IAudioSink::Format source_format_;
  IAudioSink::Format target_format_;
  size_t leftover_bytes_;
  size_t leftover_offset_;
};

}  // namespace audio
