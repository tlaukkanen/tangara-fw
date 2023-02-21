#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "chunk.hpp"
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

  auto StackSizeBytes() const -> std::size_t override { return 10 * 1024; };

  auto HasUnprocessedInput() -> bool override;
  auto IsOverBuffered() -> bool override;

  auto ProcessStreamInfo(const StreamInfo& info) -> void override;
  auto ProcessChunk(const cpp::span<std::byte>& chunk) -> void override;
  auto ProcessEndOfStream() -> void override;
  auto Process() -> void override;

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

 private:
  memory::Arena arena_;
  std::unique_ptr<codecs::ICodec> current_codec_;
  std::optional<StreamInfo> stream_info_;
  std::optional<ChunkReader> chunk_reader_;

  bool has_sent_stream_info_;
  bool has_samples_to_send_;
  bool needs_more_input_;
};

}  // namespace audio
