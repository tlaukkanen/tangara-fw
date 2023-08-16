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
#include "sink_mixer.hpp"
#include "track.hpp"
#include "types.hpp"

namespace audio {

class Timer {
 public:
  Timer(const codecs::ICodec::OutputFormat& format);

  auto AddSamples(std::size_t) -> void;

 private:
  uint32_t current_seconds_;
  uint32_t current_sample_in_second_;
  uint32_t samples_per_second_;

  uint32_t total_duration_seconds_;
};

class AudioTask {
 public:
  static auto Start(std::shared_ptr<IAudioSource> source,
                    std::shared_ptr<SinkMixer> mixer) -> AudioTask*;

  auto Main() -> void;

  AudioTask(const AudioTask&) = delete;
  AudioTask& operator=(const AudioTask&) = delete;

 private:
  AudioTask(std::shared_ptr<IAudioSource> source,
            std::shared_ptr<SinkMixer> mixer);

  auto BeginDecoding(std::shared_ptr<codecs::IStream>) -> bool;
  auto ContinueDecoding() -> bool;

  std::shared_ptr<IAudioSource> source_;
  std::shared_ptr<SinkMixer> mixer_;

  std::shared_ptr<codecs::IStream> stream_;
  std::unique_ptr<codecs::ICodec> codec_;
  std::unique_ptr<Timer> timer_;

  std::optional<codecs::ICodec::OutputFormat> current_format_;
  std::optional<IAudioOutput::Format> current_sink_format_;

  cpp::span<sample::Sample> codec_buffer_;
};

}  // namespace audio
