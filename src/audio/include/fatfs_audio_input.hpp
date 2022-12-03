#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "freertos/FreeRTOS.h"

#include "freertos/message_buffer.h"
#include "freertos/queue.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "storage.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto ProcessStreamInfo(StreamInfo& info)
      -> cpp::result<void, AudioProcessingError>;
  auto ProcessChunk(cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError> = 0;
  auto ProcessIdle() -> cpp::result<void, AudioProcessingError>;

  auto SendChunk(cpp::span<std::byte> dest) -> size_t;

 private:
  auto GetRingBufferDistance() -> size_t;

  std::shared_ptr<drivers::SdStorage> storage_;

  std::byte* raw_file_buffer_;
  cpp::span<std::byte> file_buffer_;
  cpp::span<std::byte>::iterator file_buffer_read_pos_;
  cpp::span<std::byte>::iterator pending_read_pos_;
  cpp::span<std::byte>::iterator file_buffer_write_pos_;

  std::byte* raw_chunk_buffer_;
  cpp::span<std::byte> chunk_buffer_;

  FIL current_file_;
  bool is_file_open_;

  uint8_t* output_buffer_memory_;
  StaticMessageBuffer_t output_buffer_metadata_;
};

}  // namespace audio
