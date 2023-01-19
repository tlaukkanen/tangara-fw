#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

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

  auto StackSizeBytes() const -> std::size_t override { return 8196; };

  auto InputMinChunkSize() const -> std::size_t override {
    // 128 kbps MPEG-1 @ 44.1 kHz is approx. 418 bytes according to the
    // internet.
    // TODO(jacqueline): tune as more codecs are added.
    return 1024;
  }

  auto ProcessStreamInfo(const StreamInfo& info)
      -> cpp::result<void, AudioProcessingError> override;
  auto ProcessChunk(const cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError> override;
  auto ProcessIdle() -> cpp::result<void, AudioProcessingError> override;

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

 private:
  std::unique_ptr<codecs::ICodec> current_codec_;
  std::optional<StreamInfo> stream_info_;

  std::unique_ptr<ChunkWriter> chunk_writer_;
};

}  // namespace audio
