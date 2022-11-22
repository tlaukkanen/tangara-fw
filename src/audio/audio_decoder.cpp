#include "audio_decoder.hpp"
#include <cstddef>
#include <cstdint>
#include <string.h>
#include "esp_heap_caps.h"
#include "include/audio_element.hpp"
#include "include/fatfs_audio_input.hpp"

namespace audio {

  // TODO: could this be larger? depends on the codecs i guess
  static const std::size_t kWorkingBufferSize = kMaxFrameSize;

  AudioDecoder::AudioDecoder() {
    working_buffer_ = heap_caps_malloc(kWorkingBufferSize, MALLOC_CAP_SPIRAM);
  }

  AudioDecoder::~AudioDecoder() {
    free(working_buffer_);
  }

  auto AudioDecoder::InputBuffer() -> StreamBufferHandle_t {
    return input_buffer_;
  }

  auto AudioDecoder::OutputBuffer() -> StreamBufferHandle_t {
    return output_buffer_;
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

    while (true) {
      auto result = current_codec_->Process(data, length, working_buffer_, kWorkingBufferSize);

      if (result.has_error()) {
        // TODO: handle i guess
        return ERROR;
      }
      ICodec::Result process_res = result.value();

      if (process_res.flush_output) {
        xStreamBufferSend(&output_buffer_, working_buffer_, process_res.output_written, kMaxWaitTicks);
      }

      if (process_res.need_more_input) {
        // TODO: wtf do we do about the leftover bytes?
        return OK;
      }
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
