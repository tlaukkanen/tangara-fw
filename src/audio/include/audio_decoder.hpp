#pragma once

#include <cstddef>
#include "audio_element.hpp"
#include "ff.h"
#include "codec.hpp"

namespace audio {

class AudioDecoder : public IAudioElement {
 public:
  AudioDecoder();
  ~AudioDecoder();

  auto Pause() -> void;
  auto IsPaused() -> bool;

  auto Resume() -> void;

  auto SetInputCommandQueue(QueueHandle_t) -> void;
  auto SetOutputCommandQueue(QueueHandle_t) -> void;
  auto SetInputBuffer(StreamBufferHandle_t) -> void;
  auto SetOutputBuffer(StreamBufferHandle_t) -> void;

 private:
  std::unique_ptr<codecs::ICodec> current_codec_;

  uint8_t *working_buffer_;

  QueueHandle_t input_queue_;
  QueueHandle_t output_queue_;
  StreamBufferHandle_t input_buffer_;
  StreamBufferHandle_t output_buffer_;
};

}  // namespace audio
