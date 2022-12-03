#pragma once

#include <cstddef>
#include <cstdint>

#include "ff.h"
#include "span.hpp"

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

  auto SetInputBuffer(MessageBufferHandle_t*) -> void;
  auto SetOutputBuffer(MessageBufferHandle_t*) -> void;

  auto ProcessStreamInfo(StreamInfo& info)
      -> cpp::result<void, AudioProcessingError>;
  auto ProcessChunk(cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError>;
  auto ProcessIdle() -> cpp::result<void, AudioProcessingError>;

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

 private:
  std::unique_ptr<codecs::ICodec> current_codec_;
  std::optional<StreamInfo> stream_info_;

  std::byte* raw_chunk_buffer_;
  cpp::span<std::byte> chunk_buffer_;
};

}  // namespace audio
