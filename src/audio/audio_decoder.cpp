/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_decoder.hpp"

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

static const char* kTag = "audio_dec";

static constexpr std::size_t kCodecBufferLength = 240 * 4 * 64;

Timer::Timer(const codecs::ICodec::OutputFormat& format)
    : current_seconds_(0),
      current_sample_in_second_(0),
      samples_per_second_(format.sample_rate_hz * format.num_channels),
      total_duration_seconds_(format.total_samples.value_or(0) /
                              format.num_channels / format.sample_rate_hz) {}

auto Timer::AddSamples(std::size_t samples) -> void {
  bool incremented = false;
  current_sample_in_second_ += samples;
  while (current_sample_in_second_ >= samples_per_second_) {
    current_seconds_++;
    current_sample_in_second_ -= samples_per_second_;
    incremented = true;
  }

  if (incremented) {
    if (total_duration_seconds_ < current_seconds_) {
      total_duration_seconds_ = current_seconds_;
    }

    PlaybackUpdate ev{.seconds_elapsed = current_seconds_,
                      .seconds_total = total_duration_seconds_};
    events::Audio().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
}

auto Decoder::Start(std::shared_ptr<IAudioSource> source,
                    std::shared_ptr<SampleConverter> sink) -> Decoder* {
  Decoder* task = new Decoder(source, sink);
  tasks::StartPersistent<tasks::Type::kAudioDecoder>(1,
                                                     [=]() { task->Main(); });
  return task;
}

Decoder::Decoder(std::shared_ptr<IAudioSource> source,
                 std::shared_ptr<SampleConverter> mixer)
    : source_(source),
      converter_(mixer),
      codec_(),
      timer_(),
      current_format_() {
  ESP_LOGI(kTag, "allocating codec buffer, %u KiB", kCodecBufferLength / 1024);
  codec_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kCodecBufferLength, sizeof(sample::Sample), MALLOC_CAP_SPIRAM)),
      kCodecBufferLength};
}

void Decoder::Main() {
  for (;;) {
    if (source_->HasNewStream() || !stream_) {
      std::shared_ptr<codecs::IStream> new_stream = source_->NextStream();
      if (new_stream && BeginDecoding(new_stream)) {
        stream_ = new_stream;
      } else {
        continue;
      }
    }

    if (ContinueDecoding()) {
      events::Audio().Dispatch(internal::InputFileFinished{});
      stream_.reset();
    }
  }
}

auto Decoder::BeginDecoding(std::shared_ptr<codecs::IStream> stream) -> bool {
  codec_.reset(codecs::CreateCodecForType(stream->type()).value_or(nullptr));
  if (!codec_) {
    ESP_LOGE(kTag, "no codec found");
    return false;
  }

  auto open_res = codec_->OpenStream(stream);
  if (open_res.has_error()) {
    ESP_LOGE(kTag, "codec failed to start: %s",
             codecs::ICodec::ErrorString(open_res.error()).c_str());
    return false;
  }

  if (open_res->total_samples) {
    timer_.reset(new Timer(open_res.value()));
  } else {
    timer_.reset();
  }

  current_sink_format_ = IAudioOutput::Format{
      .sample_rate = open_res->sample_rate_hz,
      .num_channels = open_res->num_channels,
      .bits_per_sample = 16,
  };
  ESP_LOGI(kTag, "stream started ok");
  events::Audio().Dispatch(internal::InputFileOpened{});
  return true;
}

auto Decoder::ContinueDecoding() -> bool {
  auto res = codec_->DecodeTo(codec_buffer_);
  if (res.has_error()) {
    return true;
  }

  if (res->samples_written > 0) {
    converter_->ConvertSamples(codec_buffer_.first(res->samples_written),
                               current_sink_format_.value(),
                               res->is_stream_finished);
  }

  if (timer_) {
    timer_->AddSamples(res->samples_written);
  }

  return res->is_stream_finished;
}

}  // namespace audio
