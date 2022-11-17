#include "audio_decoder.hpp"
#include <cstddef>
#include "esp_heap_caps.h"
#include "include/audio_element.hpp"
#include "include/fatfs_audio_input.hpp"

namespace audio {

static const TickType_t kMaxWaitTicks = portMAX_DELAY;
  // TODO: could this be larger? depends on the codecs i guess
  static const std::size_t kWorkingBufferSize = kMaxFrameSize;

  AudioDecoder::AudioDecoder() {
    working_buffer_ = heap_caps_malloc(kWorkingBufferSize, MALLOC_CAP_SPIRAM);
  }

  AudioDecoder::~AudioDecoder() {
    free(working_buffer_);
  }

  auto AudioDecoder::InputCommandQueue() -> QueueHandle_t {
    return input_queue_;
  }

  auto AudioDecoder::SetInputCommandQueue(QueueHandle_t queue) -> void {
    input_queue_ = queue;
  }

  auto AudioDecoder::SetOutputCommandQueue(QueueHandle_t queue) -> void {
    output_queue_ = queue;
  }

  auto AudioDecoder::InputBuffer() -> StreamBufferHandle_t {
    return input_buffer_;
  }

  auto AudioDecoder::SetInputBuffer(StreamBufferHandle_t buffer) -> void {
    input_buffer_ = buffer;
  }

  auto AudioDecoder::SetOutputBuffer(StreamBufferHandle_t buffer) -> void {
    output_buffer_ = buffer;
  }

  auto AudioDecoder::ProcessElementCommand(void* command) -> ProcessResult {
    FatfsAudioInput::OutputCommand *real = std::reinterpret_cast<FatfsAudioInput::OutputCommand*>(command);

    if (current_codec_->CanHandleExtension(real->extension)) {
      // TODO: Do we need to reset the codec?
      delete real;
      return OK;
    }

    auto result = codecs::CreateCodecForExtension(real->extension);
    // TODO: handle error case
    if (result.has_value()) {
      current_codec_ = result.value();
    }

    delete real;
    return OK;
  }

  auto AudioDecoder::SkipElementCommand(void* command) -> void {
    FatfsAudioInput::OutputCommand *real = std::reinterpret_cast<FatfsAudioInput::OutputCommand*>(command);
    delete real;
  }

  auto AudioDecoder::ProcessData(uint8_t* data, uint16_t length) -> ProcessResult {
    if (current_codec_ == nullptr) {
      // TODO: signal this
      return OK;
    }

    auto result = current_codec_->Process(data, length, working_buffer_, kWorkingBufferSize);
    if (result.has_value()) {
      xStreamBufferSend(&output_buffer_, working_buffer_, result.value(), kMaxWaitTicks);
    } else {
      // TODO: handle i guess
      return ERROR;
    }

    return OK;
  }

  auto AudioDecoder::ProcessIdle() -> ProcessResult {
    // Not used.
    return OK;
  }

  auto AudioDecoder::Pause() -> void {
    // TODO.
  }
  auto AudioDecoder::IsPaused() -> bool {
    // TODO.
  }

  auto AudioDecoder::Resume() -> void {
    // TODO.
  }


} // namespace audio
