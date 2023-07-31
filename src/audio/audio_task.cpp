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
#include "span.hpp"

#include "arena.hpp"
#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "sys/_stdint.h"
#include "tasks.hpp"
#include "types.hpp"
#include "ui_fsm.hpp"

namespace audio {

static const char* kTag = "audio_dec";

static constexpr std::size_t kSampleBufferSize = 16 * 1024;

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
  tasks::StartPersistent<tasks::Type::kAudio>([=]() { task->Main(); });
  return task;
}

AudioTask::AudioTask(IAudioSource* source, IAudioSink* sink)
    : source_(source),
      sink_(sink),
      codec_(),
      timer_(),
      has_begun_decoding_(false),
      current_input_format_(),
      current_output_format_(),
      sample_buffer_(reinterpret_cast<std::byte*>(
          heap_caps_malloc(kSampleBufferSize,
                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))),
      sample_buffer_len_(kSampleBufferSize) {}

void AudioTask::Main() {
  for (;;) {
    source_->Read(
        [this](IAudioSource::Flags flags, InputStream& stream) -> void {
          if (flags.is_start()) {
            has_begun_decoding_ = false;
            if (!HandleNewStream(stream)) {
              return;
            }
          }

          auto pcm = stream.info().format_as<StreamInfo::Pcm>();
          if (pcm) {
            if (ForwardPcmStream(*pcm, stream.data())) {
              stream.consume(stream.data().size_bytes());
            }
            return;
          }

          if (!stream.info().format_as<StreamInfo::Encoded>() || !codec_) {
            // Either unknown stream format, or it's encoded but we don't have
            // a decoder that supports it. Either way, bail out.
            return;
          }

          if (!has_begun_decoding_) {
            if (BeginDecoding(stream)) {
              has_begun_decoding_ = true;
            } else {
              return;
            }
          }

          // At this point the decoder has been initialised, and the sink has
          // been correctly configured. All that remains is to throw samples
          // into the sink as fast as possible.
          if (!ContinueDecoding(stream)) {
            codec_.reset();
          }

          if (flags.is_end()) {
            FinishDecoding(stream);
            events::Audio().Dispatch(internal::InputFileFinished{});
          }
        },
        portMAX_DELAY);
  }
}

auto AudioTask::HandleNewStream(const InputStream& stream) -> bool {
  // This must be a new stream of data. Reset everything to prepare to
  // handle it.
  current_input_format_ = stream.info().format();
  codec_.reset();

  // What kind of data does this new stream contain?
  auto pcm = stream.info().format_as<StreamInfo::Pcm>();
  auto encoded = stream.info().format_as<StreamInfo::Encoded>();
  if (pcm) {
    // It's already decoded! We can always handle this.
    return true;
  } else if (encoded) {
    // The stream has some kind of encoding. Whether or not we can
    // handle it is entirely down to whether or not we have a codec for
    // it.
    has_begun_decoding_ = false;
    auto codec = codecs::CreateCodecForType(encoded->type);
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
    return false;
  }
}

auto AudioTask::BeginDecoding(InputStream& stream) -> bool {
  auto res = codec_->BeginStream(stream.data());
  stream.consume(res.first);

  if (res.second.has_error()) {
    if (res.second.error() == codecs::ICodec::Error::kOutOfInput) {
      // Running out of input is fine; just return and we will try beginning the
      // stream again when we have more data.
      return false;
    }
    // Decoding the header failed, so we can't actually deal with this stream
    // after all. It could be malformed.
    ESP_LOGE(kTag, "error beginning stream");
    codec_.reset();
    return false;
  }

  codecs::ICodec::OutputFormat format = res.second.value();
  StreamInfo::Pcm new_format{
      .channels = format.num_channels,
      .bits_per_sample = format.bits_per_sample,
      .sample_rate = format.sample_rate_hz,
  };

  Duration duration;
  if (format.duration_seconds) {
    duration.src = Duration::Source::kCodec;
    duration.duration = *format.duration_seconds;
  } else if (stream.info().total_length_seconds()) {
    duration.src = Duration::Source::kLibTags;
    duration.duration = *stream.info().total_length_seconds();
  } else {
    duration.src = Duration::Source::kFileSize;
    duration.duration = *stream.info().total_length_bytes();
  }

  if (!ConfigureSink(new_format, duration)) {
    return false;
  }

  return true;
}

auto AudioTask::ContinueDecoding(InputStream& stream) -> bool {
  while (!stream.data().empty()) {
    auto res = codec_->ContinueStream(stream.data(),
                                      {sample_buffer_, sample_buffer_len_});

    stream.consume(res.first);

    if (res.second.has_error()) {
      if (res.second.error() == codecs::ICodec::Error::kOutOfInput) {
        return true;
      } else {
        return false;
      }
    } else {
      xStreamBufferSend(sink_->stream(), sample_buffer_,
                        res.second->bytes_written, portMAX_DELAY);
      timer_->AddBytes(res.second->bytes_written);
    }
  }
  return true;
}

auto AudioTask::FinishDecoding(InputStream& stream) -> void {
  // HACK: libmad requires each frame passed to it to have an additional
  // MAD_HEADER_GUARD (8) bytes after the end of the frame. Without these extra
  // bytes, it will not decode the frame.
  // The is fine for most of the stream, but at the end of the stream we don't
  // get a trailing 8 bytes for free.
  if (stream.info().format_as<StreamInfo::Encoded>()->type ==
      codecs::StreamType::kMp3) {
    ESP_LOGI(kTag, "applying MAD_HEADER_GUARD fix");

    std::unique_ptr<RawStream> mad_buffer;
    mad_buffer.reset(new RawStream(stream.data().size_bytes() + 8));

    OutputStream writer{mad_buffer.get()};
    std::copy(stream.data().begin(), stream.data().end(),
              writer.data().begin());
    std::fill(writer.data().begin(), writer.data().end(), std::byte{0});
    InputStream padded_stream{mad_buffer.get()};

    auto res = codec_->ContinueStream(stream.data(),
                                      {sample_buffer_, sample_buffer_len_});
    if (res.second.has_error()) {
      return;
    }

    xStreamBufferSend(sink_->stream(), sample_buffer_,
                      res.second->bytes_written, portMAX_DELAY);
    timer_->AddBytes(res.second->bytes_written);
  }
}

auto AudioTask::ForwardPcmStream(StreamInfo::Pcm& format,
                                 cpp::span<const std::byte> samples) -> bool {
  // First we need to reconfigure the sink for this sample format.
  if (format != current_output_format_) {
    Duration d{
        .src = Duration::Source::kFileSize,
        .duration = samples.size_bytes(),
    };
    if (!ConfigureSink(format, d)) {
      return false;
    }
  }

  // Stream the raw samples directly to the sink.
  xStreamBufferSend(sink_->stream(), samples.data(), samples.size_bytes(),
                    portMAX_DELAY);
  timer_->AddBytes(samples.size_bytes());
  return true;
}

auto AudioTask::ConfigureSink(const StreamInfo::Pcm& format,
                              const Duration& duration) -> bool {
  if (format != current_output_format_) {
    // The new format is different to the old one. Wait for the sink to drain
    // before continuing.
    while (!xStreamBufferIsEmpty(sink_->stream())) {
      ESP_LOGI(kTag, "waiting for sink stream to drain...");
      // TODO(jacqueline): Get the sink drain ISR to notify us of this
      // via semaphore instead of busy-ish waiting.
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(kTag, "configuring sink");
    if (!sink_->Configure(format)) {
      return false;
    }
  }

  current_output_format_ = format;
  timer_.reset(new Timer(format, duration));
  return true;
}

}  // namespace audio
