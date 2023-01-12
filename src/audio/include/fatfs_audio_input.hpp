#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "chunk.hpp"
#include "freertos/FreeRTOS.h"

#include "freertos/message_buffer.h"
#include "freertos/queue.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  explicit FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto ProcessStreamInfo(const StreamInfo& info)
      -> cpp::result<void, AudioProcessingError> override;
  auto ProcessChunk(const cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError> override;
  auto ProcessIdle() -> cpp::result<void, AudioProcessingError> override;

  auto SendChunk(cpp::span<std::byte> dest) -> size_t;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  auto GetRingBufferDistance() const -> size_t;

  std::shared_ptr<drivers::SdStorage> storage_;

  std::byte* raw_file_buffer_;
  cpp::span<std::byte> file_buffer_;
  cpp::span<std::byte>::iterator file_buffer_read_pos_;
  cpp::span<std::byte>::iterator pending_read_pos_;
  cpp::span<std::byte>::iterator file_buffer_write_pos_;

  FIL current_file_;
  bool is_file_open_;

  ChunkWriter chunk_writer_;
};

}  // namespace audio
