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
  struct InputCommand {
    std::string filename;
    size_t seek_to;
    bool interrupt;
  };

  struct OutputCommand {
    std::string extension;
  };

  FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto OutputCommandQueue() -> QueueHandle_t;
  auto OutputBuffer() -> StreamBufferHandle_t;

 private:
  std::shared_ptr<drivers::SdStorage> storage_;

  uint8_t *working_buffer_;

  uint8_t current_sequence_ = 0;
  FIL current_file_;
  bool is_file_open_ = false;

  uint8_t* input_queue_memory_;
  StaticQueue_t input_queue_metadata_;
  QueueHandle_t input_queue_;

  uint8_t* output_queue_memory_;
  StaticQueue_t output_queue_metadata_;
  QueueHandle_t output_queue_;

  uint8_t* output_buffer_memory_;
  StaticStreamBuffer_t output_buffer_metadata_;
  StreamBufferHandle_t output_buffer_;
};

}  // namespace audio
