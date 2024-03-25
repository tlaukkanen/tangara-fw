/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_decoder.hpp"
#include <stdint.h>

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <variant>

#include "cbor.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "i2s_dac.hpp"
#include "span.hpp"

#include "audio_converter.hpp"
#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "audio_sink.hpp"
#include "audio_source.hpp"
#include "codec.hpp"
#include "event_queue.hpp"
#include "fatfs_audio_input.hpp"
#include "sample.hpp"
#include "tasks.hpp"
#include "track.hpp"
#include "types.hpp"
#include "ui_fsm.hpp"

namespace audio {

[[maybe_unused]] static const char* kTag = "audio_dec";

static constexpr std::size_t kCodecBufferLength =
    drivers::kI2SBufferLengthFrames * sizeof(sample::Sample);

auto Decoder::Start(std::shared_ptr<IAudioSource> source,
                    std::shared_ptr<SampleConverter> sink) -> Decoder* {
  Decoder* task = new Decoder(source, sink);
  tasks::StartPersistent<tasks::Type::kAudioDecoder>([=]() { task->Main(); });
  return task;
}

Decoder::Decoder(std::shared_ptr<IAudioSource> source,
                 std::shared_ptr<SampleConverter> mixer)
    : source_(source), converter_(mixer), codec_(), current_format_() {
  ESP_LOGI(kTag, "allocating codec buffer, %u KiB", kCodecBufferLength / 1024);
  codec_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kCodecBufferLength, sizeof(sample::Sample), MALLOC_CAP_DMA)),
      kCodecBufferLength};
}

void Decoder::Main() {
  for (;;) {
    if (source_->HasNewStream() || !stream_) {
      std::shared_ptr<TaggedStream> new_stream = source_->NextStream();
      ESP_LOGI(kTag, "decoder has new stream");
      if (new_stream && BeginDecoding(new_stream)) {
        stream_ = new_stream;
      } else {
        continue;
      }
    }

    if (ContinueDecoding()) {
      stream_.reset();
    }
  }
}

auto Decoder::BeginDecoding(std::shared_ptr<TaggedStream> stream) -> bool {
  // Ensure any previous codec is freed before creating a new one.
  codec_.reset();
  codec_.reset(codecs::CreateCodecForType(stream->type()).value_or(nullptr));
  if (!codec_) {
    ESP_LOGE(kTag, "no codec found");
    events::Audio().Dispatch(internal::DecoderError{});
    return false;
  }

  auto open_res = codec_->OpenStream(stream, stream->Offset());
  if (open_res.has_error()) {
    ESP_LOGE(kTag, "codec failed to start: %s",
             codecs::ICodec::ErrorString(open_res.error()).c_str());
    events::Audio().Dispatch(internal::DecoderError{});
    return false;
  }
  stream->SetPreambleFinished();
  current_sink_format_ = IAudioOutput::Format{
      .sample_rate = open_res->sample_rate_hz,
      .num_channels = open_res->num_channels,
      .bits_per_sample = 16,
  };

  ESP_LOGI(kTag, "stream started ok");

  std::optional<uint32_t> duration;
  if (open_res->total_samples) {
    duration = open_res->total_samples.value() / open_res->num_channels /
               open_res->sample_rate_hz;
  }

  events::Audio().Dispatch(internal::DecoderOpened{
      .track = std::make_shared<TrackInfo>(TrackInfo{
          .tags = stream->tags(),
          .uri = stream->Filepath(),
          .duration = duration,
          .start_offset = stream->Offset(),
          .bitrate_kbps = open_res->sample_rate_hz,
          .encoding = stream->type(),
      }),
  });

  return true;
}

auto Decoder::ContinueDecoding() -> bool {
  auto res = codec_->DecodeTo(codec_buffer_);
  if (res.has_error()) {
    events::Audio().Dispatch(internal::DecoderError{});
    return true;
  }

  if (res->samples_written > 0) {
    converter_->ConvertSamples(codec_buffer_.first(res->samples_written),
                               current_sink_format_.value(),
                               res->is_stream_finished);
  }

  if (res->is_stream_finished) {
    events::Audio().Dispatch(internal::DecoderClosed{});
    codec_.reset();
  }

  return res->is_stream_finished;
}

}  // namespace audio
