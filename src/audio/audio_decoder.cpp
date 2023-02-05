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

static const char* kTag = "DEC";

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

  if (info.chunk_size) {
    chunk_reader_.emplace(info.chunk_size.value());
  } else {
    ESP_LOGE(kTag, "no chunk size given");
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  // Reuse the existing codec if we can. This will help with gapless playback,
  // since we can potentially just continue to decode as we were before,
  // without any setup overhead.
  if (current_codec_ != nullptr &&
      current_codec_->CanHandleFile(info.path.value_or(""))) {
    current_codec_->ResetForNewStream();
    return {};
  }

  auto result = codecs::CreateCodecForFile(info.path.value_or(""));
  if (result.has_value()) {
    current_codec_ = std::move(result.value());
  } else {
    ESP_LOGE(kTag, "no codec for this file");
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  stream_info_ = info;
  has_sent_stream_info_ = false;

  return {};
}

auto AudioDecoder::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<size_t, AudioProcessingError> {
  if (current_codec_ == nullptr || !chunk_reader_) {
    // Should never happen, but fail explicitly anyway.
    ESP_LOGW(kTag, "received chunk without chunk size or codec");
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  ESP_LOGI(kTag, "received new chunk (size %u)", chunk.size());
  current_codec_->SetInput(chunk_reader_->HandleNewData(chunk));
  needs_more_input_ = false;

  return {};
}

auto AudioDecoder::Process() -> cpp::result<void, AudioProcessingError> {
  if (has_samples_to_send_) {
    ESP_LOGI(kTag, "sending samples");
    // Writing samples is relatively quick (it's just a bunch of memcopy's), so
    // do them all at once.
    while (has_samples_to_send_ && !IsOverBuffered()) {
      if (!has_sent_stream_info_) {
        has_sent_stream_info_ = true;
        auto format = current_codec_->GetOutputFormat();
        stream_info_->bits_per_sample = format.bits_per_sample;
        stream_info_->sample_rate = format.sample_rate_hz;
        stream_info_->channels = format.num_channels;

        chunk_size_ = kSamplesPerChunk * (*stream_info_->bits_per_sample);
        stream_info_->chunk_size = chunk_size_;
        ESP_LOGI(kTag, "pcm stream chunk size: %u bytes", chunk_size_);

        auto event =
            StreamEvent::CreateStreamInfo(input_events_, *stream_info_);
        SendOrBufferEvent(std::unique_ptr<StreamEvent>(event));
      }

      auto chunk = std::unique_ptr<StreamEvent>(
          StreamEvent::CreateChunkData(input_events_, chunk_size_));
      auto write_res =
          current_codec_->WriteOutputSamples(chunk->chunk_data.bytes);
      chunk->chunk_data.bytes = chunk->chunk_data.bytes.first(write_res.first);
      has_samples_to_send_ = !write_res.second;

      if (!SendOrBufferEvent(std::move(chunk))) {
        return {};
      }
    }
    // We will process the next frame during the next call to this method.
    return {};
  }

  if (!needs_more_input_) {
    ESP_LOGI(kTag, "decoding frame");
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
