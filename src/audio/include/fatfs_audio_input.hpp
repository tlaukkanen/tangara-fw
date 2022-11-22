#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "audio_element.hpp"
#include "storage.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto OutputBuffer() -> MessageBufferHandle_t;

 private:
  std::shared_ptr<drivers::SdStorage> storage_;

  uint8_t *working_buffer_;

  FIL current_file_;
  bool is_file_open_ = false;

  MessageBufferHandle_t input_buffer_;

  uint8_t* output_buffer_memory_;
  StaticMessageBuffer_t output_buffer_metadata_;
  MessageBufferHandle_t output_buffer_;
};

}  // namespace audio
