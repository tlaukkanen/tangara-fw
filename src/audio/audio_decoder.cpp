#include "audio_decoder.hpp"

#include <string.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "freertos/FreeRTOS.h"

#include "esp_heap_caps.h"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "fatfs_audio_input.hpp"
#include "stream_info.hpp"

namespace audio {

static const std::size_t kSamplesPerChunk = 256;

AudioDecoder::AudioDecoder()
    : IAudioElement(),
      stream_info_({}),
      has_samples_to_send_(false),
      needs_more_input_(true) {}

AudioDecoder::~AudioDecoder() {}

auto AudioDecoder::HasUnprocessedInput() -> bool {
  return !needs_more_input_ || has_samples_to_send_;
}

auto AudioDecoder::ProcessStreamInfo(const StreamInfo& info)
    -> cpp::result<void, AudioProcessingError> {
  stream_info_ = info;

  if (info.ChunkSize()) {
    chunk_reader_.emplace(info.ChunkSize().value());
  } else {
    // TODO.
  }

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

  // TODO: defer until first header read, so we can give better info about
  // sample rate, chunk size, etc.
  auto downstream_info = StreamEvent::CreateStreamInfo(
      input_events_, std::make_unique<StreamInfo>(info));
  downstream_info->stream_info->BitsPerSample(32);
  downstream_info->stream_info->SampleRate(48'000);
  chunk_size_ = 128;
  downstream_info->stream_info->ChunkSize(chunk_size_);

  SendOrBufferEvent(std::move(downstream_info));

  return {};
}

auto AudioDecoder::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<size_t, AudioProcessingError> {
  if (current_codec_ == nullptr || !chunk_reader_) {
    // Should never happen, but fail explicitly anyway.
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  current_codec_->SetInput(chunk_reader_->HandleNewData(chunk));

  return {};
}

auto AudioDecoder::Process() -> cpp::result<void, AudioProcessingError> {
  if (has_samples_to_send_) {
    // Writing samples is relatively quick (it's just a bunch of memcopy's), so
    // do them all at once.
    while (has_samples_to_send_ && !IsOverBuffered()) {
      auto buffer = StreamEvent::CreateChunkData(input_events_, chunk_size_);
      auto write_res =
          current_codec_->WriteOutputSamples(buffer->chunk_data.bytes);
      buffer->chunk_data.bytes =
          buffer->chunk_data.bytes.first(write_res.first);
      has_samples_to_send_ = !write_res.second;

      if (!SendOrBufferEvent(std::move(buffer))) {
        return {};
      }
    }
    // We will process the next frame during the next call to this method.
    return {};
  }

  if (!needs_more_input_) {
    auto res = current_codec_->ProcessNextFrame();
    if (res.has_error()) {
      // todo
      return {};
    }
    needs_more_input_ = res.value();
    has_samples_to_send_ = true;

    if (needs_more_input_) {
      chunk_reader_->HandleLeftovers(current_codec_->GetInputPosition());
    }
  }

  return {};
}

}  // namespace audio
