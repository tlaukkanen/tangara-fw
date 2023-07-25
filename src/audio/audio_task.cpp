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
#include "span.hpp"

#include "arena.hpp"
#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "sys/_stdint.h"
#include "tasks.hpp"
#include "ui_fsm.hpp"

namespace audio {

static const char* kTag = "audio_dec";

static constexpr std::size_t kSampleBufferSize = 16 * 1024;

Timer::Timer(StreamInfo::Pcm format)
    : format_(format),
      last_seconds_(0),
      total_duration_seconds_(0),
      current_seconds_(0) {}

auto Timer::SetLengthSeconds(uint32_t len) -> void {
  total_duration_seconds_ = len;
}

auto Timer::SetLengthBytes(uint32_t len) -> void {
  total_duration_seconds_ = 0;
}

auto Timer::AddBytes(std::size_t bytes) -> void {
  float samples_sunk = bytes;
  samples_sunk /= format_.channels;

  // Samples must be aligned to 16 bits. The number of actual bytes per
  // sample is therefore the bps divided by 16, rounded up (align to word),
  // times two (convert to bytes).
  uint8_t bytes_per_sample = ((format_.bits_per_sample + 16 - 1) / 16) * 2;
  samples_sunk /= bytes_per_sample;

  current_seconds_ += samples_sunk / format_.sample_rate;

  uint32_t rounded = std::round(current_seconds_);
  if (rounded != last_seconds_) {
    last_seconds_ = rounded;
    events::Dispatch<PlaybackUpdate, AudioState, ui::UiState>(PlaybackUpdate{
        .seconds_elapsed = rounded,
        .seconds_total =
            total_duration_seconds_ == 0 ? rounded : total_duration_seconds_});
  }
}

auto AudioTask::Start(IAudioSource* source, IAudioSink* sink) -> AudioTask* {
  AudioTask* task = new AudioTask(source, sink);
  tasks::StartPersistent<tasks::Type::kAudio>([=]() { task->Main(); });
  return task;
}

AudioTask::AudioTask(IAudioSource* source, IAudioSink* sink)
    : source_(source),
      sink_(sink),
      codec_(),
      timer_(),
      is_new_stream_(false),
      current_input_format_(),
      current_output_format_(),
      sample_buffer_(reinterpret_cast<std::byte*>(
          heap_caps_malloc(kSampleBufferSize,
                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))),
      sample_buffer_len_(kSampleBufferSize) {}

void AudioTask::Main() {
  for (;;) {
    source_->Read(
        [this](StreamInfo::Format format) -> bool {
          if (current_input_format_ && format == *current_input_format_) {
            // This is the continuation of previous data. We can handle it if
            // we are able to decode it, or if it doesn't need decoding.
            return current_output_format_ == format || codec_ != nullptr;
          }
          // This must be a new stream of data. Reset everything to prepare to
          // handle it.
          current_input_format_ = format;
          is_new_stream_ = true;
          codec_.reset();
          timer_.reset();

          // What kind of data does this new stream contain?
          if (std::holds_alternative<StreamInfo::Pcm>(format)) {
            // It's already decoded! We can handle this immediately if it
            // matches what we're currently sending to the sink. Otherwise, we
            // will need to wait for the sink to drain before we can reconfigure
            // it.
            if (current_output_format_ && format == *current_output_format_) {
              return true;
            } else if (xStreamBufferIsEmpty(sink_->stream())) {
              return true;
            } else {
              return false;
            }
          } else if (std::holds_alternative<StreamInfo::Encoded>(format)) {
            // The stream has some kind of encoding. Whether or not we can
            // handle it is entirely down to whether or not we have a codec for
            // it.
            auto encoding = std::get<StreamInfo::Encoded>(format);
            auto codec = codecs::CreateCodecForType(encoding.type);
            if (codec) {
              ESP_LOGI(kTag, "successfully created codec for stream");
              codec_.reset(*codec);
              return true;
            } else {
              ESP_LOGE(kTag, "stream has unknown encoding");
              return false;
            }
          } else {
            // programmer error / skill issue :(
            ESP_LOGE(kTag, "stream has unknown format");
            current_input_format_ = format;
            return false;
          }
        },
        [this](cpp::span<const std::byte> bytes) -> size_t {
          // PCM streams are simple, so handle them first.
          if (std::holds_alternative<StreamInfo::Pcm>(*current_input_format_)) {
            // First we need to reconfigure the sink for this sample format.
            // TODO(jacqueline): We should verify whether or not the sink can
            // actually deal with this format first.
            if (current_input_format_ != current_output_format_) {
              current_output_format_ = current_input_format_;
              sink_->Configure(*current_output_format_);
              timer_.reset(new Timer(
                  std::get<StreamInfo::Pcm>(*current_output_format_)));
            }
            // Stream the raw samples directly to the sink.
            xStreamBufferSend(sink_->stream(), bytes.data(), bytes.size_bytes(),
                              portMAX_DELAY);
            timer_->AddBytes(bytes.size_bytes());
            return bytes.size_bytes();
          }
          // Else, assume it's an encoded stream.

          size_t bytes_used = 0;
          if (is_new_stream_) {
            // This is a new stream! First order of business is verifying that
            // we can indeed decode it.
            auto res = codec_->BeginStream(bytes);
            bytes_used += res.first;

            if (res.second.has_error()) {
              if (res.second.error() != codecs::ICodec::Error::kOutOfInput) {
                // Decoding the header failed, so we can't actually deal with
                // this stream after all. It could be malformed.
                ESP_LOGE(kTag, "error beginning stream");
                codec_.reset();
              }
              return bytes_used;
            }
            is_new_stream_ = false;

            codecs::ICodec::OutputFormat format = res.second.value();
            StreamInfo::Pcm pcm{
                .channels = format.num_channels,
                .bits_per_sample = format.bits_per_sample,
                .sample_rate = format.sample_rate_hz,
            };
            StreamInfo::Format new_format{pcm};
            timer_.reset(new Timer{pcm});
            if (format.duration_seconds) {
              timer_->SetLengthSeconds(*format.duration_seconds);
            }

            // Now that we have the output format for decoded samples from this
            // stream, we need to see if they are compatible with what's already
            // in the sink stream.
            if (new_format != current_output_format_) {
              // The new format is different to the old one. Wait for the sink
              // to drain before continuing.
              while (!xStreamBufferIsEmpty(sink_->stream())) {
                ESP_LOGI(kTag, "waiting for sink stream to drain...");
                // TODO(jacqueline): Get the sink drain ISR to notify us of this
                // via semaphore instead of busy-ish waiting.
                vTaskDelay(pdMS_TO_TICKS(100));
              }
            }

            ESP_LOGI(kTag, "configuring sink");
            current_output_format_ = new_format;
            sink_->Configure(new_format);
            timer_.reset(
                new Timer(std::get<StreamInfo::Pcm>(*current_output_format_)));
          }

          // At this point the decoder has been initialised, and the sink has
          // been correctly configured. All that remains is to throw samples
          // into the sink as fast as possible.
          while (bytes_used < bytes.size_bytes()) {
            auto res =
                codec_->ContinueStream(bytes.subspan(bytes_used),
                                       {sample_buffer_, sample_buffer_len_});

            bytes_used += res.first;

            if (res.second.has_error()) {
              return bytes_used;
            } else {
              xStreamBufferSend(sink_->stream(), sample_buffer_,
                                res.second->bytes_written, portMAX_DELAY);
              timer_->AddBytes(res.second->bytes_written);
            }
          }

          return bytes_used;
        },
        portMAX_DELAY);
  }
}

}  // namespace audio
