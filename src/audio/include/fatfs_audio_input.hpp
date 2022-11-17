#pragma once

#include <cstdint>
#include <memory>

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
  };

  struct OutputCommand {
    // TODO: does this actually need any special output?
  };

  FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto OutputCommandQueue() -> QueueHandle_t;
  auto OutputBuffer() -> StreamBufferHandle_t;

 private:
  std::shared_ptr<drivers::SdStorage> storage_;

  uint8_t current_sequence = 0;

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
