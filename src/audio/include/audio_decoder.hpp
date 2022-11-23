#pragma once

#include <cstddef>

#include "ff.h"

#include "audio_element.hpp"
#include "codec.hpp"

namespace audio {

/*
 * An audio element that accepts various kinds of encoded audio streams as
 * input, and converts them to uncompressed PCM output.
 */
class AudioDecoder : public IAudioElement {
 public:
  AudioDecoder();
  ~AudioDecoder();

  auto SetInputBuffer(StreamBufferHandle_t*) -> void;
  auto SetOutputBuffer(StreamBufferHandle_t*) -> void;

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

 private:
  std::unique_ptr<codecs::ICodec> current_codec_;
  std::optional<StreamInfo> stream_info_;

  uint8_t* chunk_buffer_;
};

}  // namespace audio
