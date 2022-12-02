#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "freertos/FreeRTOS.h"

#include "freertos/message_buffer.h"
#include "freertos/queue.h"

#include "audio_element.hpp"
#include "storage.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto ProcessStreamInfo(StreamInfo& info)
      -> cpp::result<void, AudioProcessingError>;
  auto ProcessChunk(uint8_t* data, std::size_t length)
      -> cpp::result<size_t, AudioProcessingError>;
  auto ProcessIdle() -> cpp::result<void, AudioProcessingError>;

  auto SendChunk(uint8_t* buffer, size_t size) -> size_t;

 private:
  auto GetRingBufferDistance() -> size_t;

  std::shared_ptr<drivers::SdStorage> storage_;

  uint8_t* file_buffer_;
  uint8_t* file_buffer_read_pos_;
  uint8_t* pending_read_pos_;
  uint8_t* file_buffer_write_pos_;

  uint8_t* chunk_buffer_;

  FIL current_file_;
  bool is_file_open_ = false;

  MessageBufferHandle_t input_buffer_;

  uint8_t* output_buffer_memory_;
  StaticMessageBuffer_t output_buffer_metadata_;
};

}  // namespace audio
