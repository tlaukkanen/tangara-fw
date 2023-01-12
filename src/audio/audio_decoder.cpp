#include "audio_decoder.hpp"

#include <string.h>

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "esp_heap_caps.h"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "fatfs_audio_input.hpp"

static const char* kTag = "DEC";

namespace audio {

AudioDecoder::AudioDecoder() : IAudioElement(), stream_info_({}) {}

AudioDecoder::~AudioDecoder() {}

auto AudioDecoder::ProcessStreamInfo(const StreamInfo& info)
    -> cpp::result<void, AudioProcessingError> {
  stream_info_ = info;

  // Reuse the existing codec if we can. This will help with gapless playback,
  // since we can potentially just continue to decode as we were before,
  // without any setup overhead.
  if (current_codec_->CanHandleFile(info.Path().value_or(""))) {
    current_codec_->ResetForNewStream();
    return {};
  }

  auto result = codecs::CreateCodecForFile(info.Path().value());
  if (result.has_value()) {
    current_codec_ = std::move(result.value());
  } else {
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  return {};
}

auto AudioDecoder::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<size_t, AudioProcessingError> {
  if (current_codec_ == nullptr) {
    // Should never happen, but fail explicitly anyway.
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  current_codec_->SetInput(chunk);

  bool has_samples_to_send = false;
  bool needs_more_input = false;
  std::optional<codecs::ICodec::ProcessingError> error = std::nullopt;
  while (1) {
    ChunkWriteResult res = chunk_writer_.WriteChunkToStream(
        [&](cpp::span<std::byte> buffer) -> std::size_t {
          std::size_t bytes_written = 0;
          // Continue filling up the output buffer so long as we have samples
          // leftover, or are able to synthesize more samples from the input.
          while (has_samples_to_send || !needs_more_input) {
            if (!has_samples_to_send) {
              auto result = current_codec_->ProcessNextFrame();
              has_samples_to_send = true;
              if (result.has_error()) {
                error = result.error();
                // End our output stream immediately if the codec barfed.
                return 0;
              } else {
                needs_more_input = result.value();
              }
            } else {
              auto result = current_codec_->WriteOutputSamples(
                  buffer.last(buffer.size() - bytes_written));
              bytes_written += result.first;
              has_samples_to_send = !result.second;
            }
          }
          return bytes_written;
        },
        // TODO
        portMAX_DELAY);

    switch (res) {
      case CHUNK_WRITE_OKAY:
        break;
      case CHUNK_WRITE_TIMEOUT:
      case CHUNK_OUT_OF_DATA:
        return {};
      default:
        return cpp::fail(IO_ERROR);
    }
  }

  if (error) {
    ESP_LOGE(kTag, "Codec encountered error %d", error.value());
    return cpp::fail(IO_ERROR);
  }

  return current_codec_->GetInputPosition();
}

auto AudioDecoder::ProcessIdle() -> cpp::result<void, AudioProcessingError> {
  // Not used; we delay forever when waiting on IO.
  return {};
}

}  // namespace audio
