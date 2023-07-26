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
#include "stream_info.hpp"

namespace audio {

class Timer {
 public:
  explicit Timer(StreamInfo::Pcm);

  auto SetLengthSeconds(uint32_t) -> void;
  auto SetLengthBytes(uint32_t) -> void;

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

 private:
  AudioTask(IAudioSource* source, IAudioSink* sink);

  auto HandleNewStream(const InputStream&) -> bool;

  auto BeginDecoding(InputStream&) -> bool;
  auto ContinueDecoding(InputStream&) -> bool;
  auto FinishDecoding(InputStream&) -> void;

  auto ForwardPcmStream(StreamInfo::Pcm&, cpp::span<const std::byte>) -> bool;

  auto ConfigureSink(const StreamInfo::Pcm&) -> bool;

  IAudioSource* source_;
  IAudioSink* sink_;
  std::unique_ptr<codecs::ICodec> codec_;
  std::unique_ptr<Timer> timer_;

  bool has_begun_decoding_;
  std::optional<StreamInfo::Format> current_input_format_;
  std::optional<StreamInfo::Pcm> current_output_format_;

  std::byte* sample_buffer_;
  std::size_t sample_buffer_len_;
};

}  // namespace audio
