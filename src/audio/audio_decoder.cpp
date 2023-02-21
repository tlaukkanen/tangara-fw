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

static const std::size_t kChunkSize = 1024;
static const std::size_t kReadahead = 8;

AudioDecoder::AudioDecoder()
    : IAudioElement(),
      arena_(kChunkSize, kReadahead, MALLOC_CAP_SPIRAM),
      stream_info_({}),
      has_samples_to_send_(false),
      needs_more_input_(true) {}

AudioDecoder::~AudioDecoder() {}

auto AudioDecoder::HasUnprocessedInput() -> bool {
  return !needs_more_input_ || has_samples_to_send_;
}

auto AudioDecoder::IsOverBuffered() -> bool {
  return arena_.BlocksFree() == 0;
}

auto AudioDecoder::ProcessStreamInfo(const StreamInfo& info) -> void {
  stream_info_ = info;

  if (info.chunk_size) {
    chunk_reader_.emplace(info.chunk_size.value());
  } else {
    ESP_LOGE(kTag, "no chunk size given");
    return;
  }

  // Reuse the existing codec if we can. This will help with gapless playback,
  // since we can potentially just continue to decode as we were before,
  // without any setup overhead.
  if (current_codec_ != nullptr &&
      current_codec_->CanHandleFile(info.path.value_or(""))) {
    current_codec_->ResetForNewStream();
    return;
  }

  auto result = codecs::CreateCodecForFile(info.path.value_or(""));
  if (result.has_value()) {
    current_codec_ = std::move(result.value());
  } else {
    ESP_LOGE(kTag, "no codec for this file");
    return;
  }

  stream_info_ = info;
  has_sent_stream_info_ = false;
}

auto AudioDecoder::ProcessChunk(const cpp::span<std::byte>& chunk) -> void {
  if (current_codec_ == nullptr || !chunk_reader_) {
    // Should never happen, but fail explicitly anyway.
    ESP_LOGW(kTag, "received chunk without chunk size or codec");
    return;
  }

  ESP_LOGI(kTag, "received new chunk (size %u)", chunk.size());
  current_codec_->SetInput(chunk_reader_->HandleNewData(chunk));
  needs_more_input_ = false;
}

auto AudioDecoder::ProcessEndOfStream() -> void {
  has_samples_to_send_ = false;
  needs_more_input_ = true;
  current_codec_.reset();

  SendOrBufferEvent(std::unique_ptr<StreamEvent>(
      StreamEvent::CreateEndOfStream(input_events_)));
}

auto AudioDecoder::Process() -> void {
  if (has_samples_to_send_) {
    // Writing samples is relatively quick (it's just a bunch of memcopy's), so
    // do them all at once.
    while (has_samples_to_send_ && !IsOverBuffered()) {
      if (!has_sent_stream_info_) {
        has_sent_stream_info_ = true;
        auto format = current_codec_->GetOutputFormat();
        stream_info_->bits_per_sample = format.bits_per_sample;
        stream_info_->sample_rate = format.sample_rate_hz;
        stream_info_->channels = format.num_channels;
        stream_info_->chunk_size = kChunkSize;

        auto event =
            StreamEvent::CreateStreamInfo(input_events_, *stream_info_);
        SendOrBufferEvent(std::unique_ptr<StreamEvent>(event));
      }

      auto block = arena_.Acquire();
      if (!block) {
        return;
      }

      auto write_res =
          current_codec_->WriteOutputSamples({block->start, block->size});
      block->used_size = write_res.first;
      has_samples_to_send_ = !write_res.second;

      auto chunk = std::unique_ptr<StreamEvent>(
          StreamEvent::CreateArenaChunk(input_events_, *block));
      if (!SendOrBufferEvent(std::move(chunk))) {
        return;
      }
    }
    // We will process the next frame during the next call to this method.
  }

  if (!needs_more_input_) {
    auto res = current_codec_->ProcessNextFrame();
    if (res.has_error()) {
      // TODO(jacqueline): Handle errors.
      return;
    }
    needs_more_input_ = res.value();
    has_samples_to_send_ = true;

    if (needs_more_input_) {
      chunk_reader_->HandleBytesUsed(current_codec_->GetInputPosition());
    }
  }
}

}  // namespace audio
