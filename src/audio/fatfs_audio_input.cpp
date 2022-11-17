#include "fatfs_audio_input.hpp<D-c>ccc
#include <cstdint>
#include <memory>

#include "esp_heap_caps.h"

#include "audio_element.hpp"
#include "freertos/portmacro.h"
#include "include/audio_element.hpp"

namespace audio {

static const TickType_t kMaxWaitTicks = portMAX_DELAY;

// Large output buffer size, so that we can keep a get as much of the input file
// into memory as soon as possible.
static constexpr std::size_t kOutputBufferSize = 1024 * 128;
static constexpr std::size_t kQueueItemSize = sizeof(IAudioElement::Command);
// Use a large enough command queue size that we can fit reads for the full
// buffer into the queue.
static constexpr std::size_t kOutputQueueItemNumber = kOutputBufferSize / kMaxFrameSize;
static constexpr std::size_t kOutputQueueSize = kOutputQueueItemNumber * kQueueItemSize;

// This should be a relatively responsive element, so no need for a particularly
// large queue.
static constexpr std::size_t kInputQueueItemNumber = 4;
static constexpr std::size_t kInputQueueSize = kInputQueueItemNumber * kQueueItemSize;

FatfsAudioInput::FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage)
    : IAudioElement(), storage_(storage) {
  working_buffer_ = heap_caps_malloc(kMaxFrameSize, MALLOC_CAP_SPIRAM);

  input_queue_memory_ = heap_caps_malloc(kInputQueueSize, MALLOC_CAP_SPIRAM);
  input_queue_ = xQueueCreateStatic(
      kInputQueueItemNumber, kQueueItemSize, input_queue_memory_, &input_queue_metadata_);

  output_queue_memory_ = heap_caps_malloc(kOutputQueueSize, MALLOC_CAP_SPIRAM);
  output_queue_ =
      xQueueCreateStatic(kOutputQueueItems, kQueueItemSize, output_queue_memory_,
                         &output_queue_metadata_);

  output_buffer_memory_ =
      heap_caps_malloc(kOutputBufferSize, MALLOC_CAP_SPIRAM);
  output_buffer_ =
      xStreamBufferCreateStatic(kOutputBufferSize - 1, 1, output_buffer_memory_,
                                &output_buffer_metadata_);
}

FatfsAudioInput::~FatfsAudioInput() {
  free(working_buffer_);
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

auto FatfsAudioInput::ProcessElementCommand(void* command) -> ProcessResult {
  InputCommand *real = std::reinterpret_cast<InputCommand*>(command);

  if (uxQueueSpacesAvailable(output_queue_) < 2) {
    return OUTPUT_FULL;
  }

  if (is_file_open_) {
    f_close(&current_file_);
  }

  FRESULT res = f_open(&current_file_, real->filename.c_str(), FA_READ);
  if (res != FR_OK) {
    delete real;
    return ERROR;
  }

  if (real->seek_to && f_lseek(&current_file_, real->seek_to) {
      return ERROR;
  }

  is_file_open_ = true;
  current_sequence_++;

  Command sequence_update;
  sequence_update.type = SEQUENCE_NUMBER;
  sequence_update.sequence_number = current_sequence_;

  if (real->interrupt) {
    xQueueSendToFront(output_queue_, &sequence_update, kMaxWaitTicks);
  } else {
    xQueueSendToBack(output_queue_, &sequence_update, kMaxWaitTicks);
  }

  OutputCommand *data = new OutputCommand;
  data->extension = "txt";
  Command file_info;
  file_info.type = ELEMENT;
  file_info.sequence_number = current_sequence_;
  file_info.data = &data;
  xQueueSendToBack(output_queue_, &file_info, kMaxWaitTicks);

  delete real;
  return OK;
}

auto FatfsAudioInput::SkipElementCommand(void* command) -> void {
  InputCommand *real = std::reinterpret_cast<input_key_service_add_key*>(command);
  delete real;
}

auto FatfsAudioInput::ProcessData(uint8_t* data, uint16_t length) -> void {
  // Not used, since we have no input stream.
}

auto FatfsAudioInput::ProcessIdle() -> ProcessResult {
  if (!is_file_open_) {
    return OK;
  }

  if (xStreamBufferSpacesAvailable(output_buffer) < kMaxFrameSize) {
    return OUTPUT_FULL;
  }

  UINT bytes_read = 0;
  FRESULT result = f_read(&current_file_, working_buffer_, kMaxFrameSize, &bytes_read);
  if (!FR_OK) {
    return ERROR;
  }

  xStreamBufferSend(&output_buffer_, working_buffer_, bytes_read, kMaxWaitTicks);

  if (f_eof(&current_file_)) {
    f_close(&current_file_);
    is_file_open_ = false;
  }

  return OK;
}

}  // namespace audio
