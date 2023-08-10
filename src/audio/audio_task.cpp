/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_task.hpp"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <variant>

#include "audio_decoder.hpp"
#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "audio_sink.hpp"
#include "audio_source.hpp"
#include "cbor.h"
#include "codec.hpp"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "fatfs_audio_input.hpp"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "pipeline.hpp"
#include "sample.hpp"
#include "sink_mixer.hpp"
#include "span.hpp"

#include "arena.hpp"
#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "sys/_stdint.h"
#include "tasks.hpp"
#include "track.hpp"
#include "types.hpp"
#include "ui_fsm.hpp"

namespace audio {

static const char* kTag = "audio_dec";

static constexpr std::size_t kCodecBufferLength = 240 * 4;

Timer::Timer(const StreamInfo::Pcm& format, const Duration& duration)
    : format_(format), current_seconds_(0), current_sample_in_second_(0) {
  switch (duration.src) {
    case Duration::Source::kLibTags:
      ESP_LOGI(kTag, "using duration from libtags");
      total_duration_seconds_ = duration.duration;
      break;
    case Duration::Source::kCodec:
      ESP_LOGI(kTag, "using duration from decoder");
      total_duration_seconds_ = duration.duration;
      break;
    case Duration::Source::kFileSize:
      ESP_LOGW(kTag, "calculating duration from filesize");
      total_duration_seconds_ =
          bytes_to_samples(duration.duration) / format_.sample_rate;
      break;
  }
}

auto Timer::AddBytes(std::size_t bytes) -> void {
  bool incremented = false;
  current_sample_in_second_ += bytes_to_samples(bytes);
  while (current_sample_in_second_ >= format_.sample_rate) {
    current_seconds_++;
    current_sample_in_second_ -= format_.sample_rate;
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

auto Timer::bytes_to_samples(uint32_t bytes) -> uint32_t {
  uint32_t samples = bytes;
  samples /= format_.channels;

  // Samples must be aligned to 16 bits. The number of actual bytes per
  // sample is therefore the bps divided by 16, rounded up (align to word),
  // times two (convert to bytes).
  uint8_t bytes_per_sample = ((format_.bits_per_sample + 16 - 1) / 16) * 2;
  samples /= bytes_per_sample;
  return samples;
}

auto AudioTask::Start(IAudioSource* source, IAudioSink* sink) -> AudioTask* {
  AudioTask* task = new AudioTask(source, sink);
  // Pin to CORE1 because codecs should be fixed point anyway, and being on
  // the opposite core to the mixer maximises throughput in the worst case
  // (some heavy codec like opus + resampling for bluetooth).
  tasks::StartPersistent<tasks::Type::kAudio>(1, [=]() { task->Main(); });
  return task;
}

AudioTask::AudioTask(IAudioSource* source, IAudioSink* sink)
    : source_(source),
      sink_(sink),
      codec_(),
      mixer_(new SinkMixer(sink)),
      timer_(),
      current_format_() {
  codec_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kCodecBufferLength, sizeof(sample::Sample), MALLOC_CAP_SPIRAM)),
      kCodecBufferLength};
}

void AudioTask::Main() {
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

auto AudioTask::BeginDecoding(std::shared_ptr<codecs::IStream> stream) -> bool {
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

  current_sink_format_ = IAudioSink::Format{
      .sample_rate = open_res->sample_rate_hz,
      .num_channels = open_res->num_channels,
      .bits_per_sample = 32,
  };
  ESP_LOGI(kTag, "stream started ok");
  events::Audio().Dispatch(internal::InputFileOpened{});
  return true;
}

auto AudioTask::ContinueDecoding() -> bool {
  auto res = codec_->DecodeTo(codec_buffer_);
  if (res.has_error()) {
    return true;
  }

  if (res->samples_written > 0) {
    mixer_->MixAndSend(codec_buffer_.first(res->samples_written),
                       current_sink_format_.value(), res->is_stream_finished);
  }

  return res->is_stream_finished;
}

}  // namespace audio
