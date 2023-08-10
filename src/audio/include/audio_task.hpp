/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
#include <cstdint>
#include <memory>
#include "audio_decoder.hpp"
#include "audio_sink.hpp"
#include "audio_source.hpp"
#include "codec.hpp"
#include "pipeline.hpp"
#include "sink_mixer.hpp"
#include "stream_info.hpp"
#include "track.hpp"
#include "types.hpp"

namespace audio {

struct Duration {
  enum class Source {
    kLibTags,
    kCodec,
    kFileSize,
  };
  Source src;
  uint32_t duration;
};

class Timer {
 public:
  Timer(const StreamInfo::Pcm&, const Duration&);

  auto AddBytes(std::size_t) -> void;

 private:
  auto bytes_to_samples(uint32_t) -> uint32_t;

  StreamInfo::Pcm format_;

  uint32_t current_seconds_;
  uint32_t current_sample_in_second_;

  uint32_t total_duration_seconds_;
};

class AudioTask {
 public:
  static auto Start(IAudioSource* source, IAudioSink* sink) -> AudioTask*;

  auto Main() -> void;

  AudioTask(const AudioTask&) = delete;
  AudioTask& operator=(const AudioTask&) = delete;

 private:
  AudioTask(IAudioSource* source, IAudioSink* sink);

  auto BeginDecoding(std::shared_ptr<codecs::IStream>) -> bool;
  auto ContinueDecoding() -> bool;

  IAudioSource* source_;
  IAudioSink* sink_;

  std::shared_ptr<codecs::IStream> stream_;
  std::unique_ptr<codecs::ICodec> codec_;
  std::unique_ptr<SinkMixer> mixer_;
  std::unique_ptr<Timer> timer_;

  std::optional<codecs::ICodec::OutputFormat> current_format_;
  std::optional<IAudioSink::Format> current_sink_format_;

  cpp::span<sample::Sample> codec_buffer_;
};

}  // namespace audio
