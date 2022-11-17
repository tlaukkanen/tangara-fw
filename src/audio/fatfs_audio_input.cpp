#include "fatfs_audio_input.hpp"
#include <memory>

#include "esp-adf/components/input_key_service/include/input_key_service.h"
#include "esp_heap_caps.h"

#include "audio_element.hpp"

namespace audio {

static const size_t kQueueItems = 0;
static constexpr size_t kQueueItemSize = sizeof(IAudioElement::Command);
static constexpr size_t kQueueSize = kQueueItems * kQueueItemSize;

static const size_t kOutputBufferSize = 1024;

FatfsAudioInput::FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage)
    : IAudioElement(), storage_(storage) {
  input_queue_memory_ = heap_caps_malloc(kQueueSize, MALLOC_CAP_SPIRAM);
  input_queue_ = xQueueCreateStatic(
      kQueueItems, kQueueItemSize, input_queue_memory_, &input_queue_metadata_);

  output_queue_memory_ = heap_caps_malloc(kQueueSize, MALLOC_CAP_SPIRAM);
  output_queue_ =
      xQueueCreateStatic(kQueueItems, kQueueItemSize, output_queue_memory_,
                         &output_queue_metadata_);

  output_buffer_memory_ =
      heap_caps_malloc(kOutputBufferSize, MALLOC_CAP_SPIRAM);
  output_buffer_ =
      xStreamBufferCreateStatic(kOutputBufferSize - 1, 1, output_buffer_memory_,
                                &output_buffer_metadata_);
}

FatfsAudioInput::~FatfsAudioInput() {
  vStreamBufferDelete(output_buffer_);
  free(output_buffer_memory_);
  vQueueDelete(output_queue_);
  free(output_queue_memory_);
  vQueueDelete(input_queue_);
  free(input_queue_memory_);
}

auto FatfsAudioInput::InputCommandQueue() -> QueueHandle_t {
  return input_queue_;
}

auto FatfsAudioInput::OutputCommandQueue() -> QueueHandle_t {
  return output_queue_;
}

auto FatfsAudioInput::InputBuffer() -> StreamBufferHandle_t {
  return nullptr;
}

auto FatfsAudioInput::OutputBuffer() -> StreamBufferHandle_t {
  return output_buffer_;
}

auto FatfsAudioInput::ProcessElementCommand(void* command) -> void {
  InputCommand *real = std::reinterpret_pointer_cast<input_key_service_add_key*>(command);

  // TODO.
}

auto FatfsAudioInput::SkipElementCommand(void* command) -> void {
  InputCommand *real = std::reinterpret_pointer_cast<input_key_service_add_key*>(command);
  delete real;
}

auto FatfsAudioInput::ProcessData(uint8_t* data, uint16_t length) -> void {
  // Not implemented.
}

auto FatfsAudioInput::ProcessIdle() -> void {
  // TODO.
}

}  // namespace audio
