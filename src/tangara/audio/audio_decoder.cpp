/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/audio_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <span>
#include <variant>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"

#include "audio/audio_converter.hpp"
#include "audio/audio_events.hpp"
#include "audio/audio_fsm.hpp"
#include "audio/audio_sink.hpp"
#include "audio/audio_source.hpp"
#include "audio/fatfs_audio_input.hpp"
#include "codec.hpp"
#include "database/track.hpp"
#include "drivers/i2s_dac.hpp"
#include "events/event_queue.hpp"
#include "sample.hpp"
#include "tasks.hpp"
#include "types.hpp"
#include "ui/ui_fsm.hpp"

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
    ESP_LOGE(kTag, "no codec found for stream");
    return false;
  }

  auto open_res = codec_->OpenStream(stream, stream->Offset());
  if (open_res.has_error()) {
    ESP_LOGE(kTag, "codec failed to start: %s",
             codecs::ICodec::ErrorString(open_res.error()).c_str());
    return false;
  }
  stream->SetPreambleFinished();
  current_sink_format_ = IAudioOutput::Format{
      .sample_rate = open_res->sample_rate_hz,
      .num_channels = open_res->num_channels,
      .bits_per_sample = 16,
  };

  std::optional<uint32_t> duration;
  if (open_res->total_samples) {
    duration = open_res->total_samples.value() / open_res->num_channels /
               open_res->sample_rate_hz;
  }

  converter_->beginStream(std::make_shared<TrackInfo>(TrackInfo{
      .tags = stream->tags(),
      .uri = stream->Filepath(),
      .duration = duration,
      .start_offset = stream->Offset(),
      .bitrate_kbps = open_res->sample_rate_hz,
      .encoding = stream->type(),
      .format = *current_sink_format_,
  }));

  return true;
}

auto Decoder::ContinueDecoding() -> bool {
  auto res = codec_->DecodeTo(codec_buffer_);
  if (res.has_error()) {
    converter_->endStream();
    return true;
  }

  if (res->samples_written > 0) {
    converter_->continueStream(codec_buffer_.first(res->samples_written));
  }

  if (res->is_stream_finished) {
    converter_->endStream();
    codec_.reset();
  }

  return res->is_stream_finished;
}

}  // namespace audio
