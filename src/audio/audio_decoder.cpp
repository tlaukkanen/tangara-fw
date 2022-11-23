#include "audio_decoder.hpp"
#include <string.h>
#include <cstddef>
#include <cstdint>
#include "chunk.hpp"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "include/audio_element.hpp"
#include "include/fatfs_audio_input.hpp"

namespace audio {

AudioDecoder::AudioDecoder()
    : IAudioElement(),
      chunk_buffer_(heap_caps_malloc(kMaxChunkSize, MALLOC_CAP_SPIRAM)),
      stream_info_({}) {}

AudioDecoder::~AudioDecoder() {
  free(chunk_buffer_);
}

auto AudioDecoder::SetInputBuffer(StreamBufferHandle_t* buffer) -> void {
  input_buffer_ = buffer;
}

auto AudioDecoder::SetOutputBuffer(StreamBufferHandle_t* buffer) -> void {
  output_buffer_ = buffer;
}

auto AudioDecoder::ProcessStreamInfo(StreamInfo&& info)
    -> cpp::result<void, StreamError> {
  stream_info_ = info;

  // Reuse the existing codec if we can. This will help with gapless playback,
  // since we can potentially just continue to decode as we were before,
  // without any setup overhead.
  if (current_codec_->CanHandleFile(info.path)) {
    current_codec_->ResetForNewStream();
    return {};
  }

  auto result = codecs::CreateCodecForFile(info.path);
  if (result.has_value()) {
    current_codec_ = std::move(result.value());
  } else {
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  return {};
}

auto AudioDecoder::ProcessChunk(uint8_t* data, std::size_t length)
    -> cpp::result<size_t, StreamError> {
  if (current_codec_ == nullptr) {
    // Should never happen, but fail explicitly anyway.
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  current_codec_->SetInput(data, length);
  cpp::result<size_t, codecs::ICodec::ProcessingError> result;
  WriteChunksToStream(
      output_buffer_, working_buffer_, kWorkingBufferSize,
      [&](uint8_t* buf, size_t len) {
        result = current_codec_->Process(data, length, buf, len);
        if (result.has_error()) {
          // End our output stream immediately if the codec barfed.
          return 0;
        }
        return result.value();
      },
      // This element doesn't support any kind of out of band commands, so we
      // can just suspend the whole task if the output buffer fills up.
      portMAX_DELAY);

  if (result.has_error()) {
    return cpp::fail(IO_ERROR);
  }

  return current_codec_->GetOutputProcessed();
}

auto AudioDecoder::ProcessIdle() -> cpp::result<void, StreamError> {
  // Not used; we delay forever when waiting on IO.
  return {};
}

}  // namespace audio
