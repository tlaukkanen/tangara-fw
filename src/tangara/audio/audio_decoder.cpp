/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/audio_decoder.hpp"

#include <cassert>
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

#include "audio/audio_events.hpp"
#include "audio/audio_fsm.hpp"
#include "audio/audio_sink.hpp"
#include "audio/audio_source.hpp"
#include "audio/processor.hpp"
#include "codec.hpp"
#include "database/track.hpp"
#include "drivers/i2s_dac.hpp"
#include "events/event_queue.hpp"
#include "sample.hpp"
#include "tasks.hpp"
#include "types.hpp"
#include "ui/ui_fsm.hpp"

namespace audio {

static const char* kTag = "decoder";

/*
 * The size of the buffer used for holding decoded samples. This buffer is
 * allocated in internal memory for greater speed, so be careful when
 * increasing its size.
 */
static constexpr std::size_t kCodecBufferLength =
    drivers::kI2SBufferLengthFrames * 2;

auto Decoder::Start(std::shared_ptr<SampleProcessor> sink) -> Decoder* {
  Decoder* task = new Decoder(sink);
  tasks::StartPersistent<tasks::Type::kAudioDecoder>([=]() { task->Main(); });
  return task;
}

auto Decoder::open(std::shared_ptr<TaggedStream> stream) -> void {
  NextStream* next = new NextStream();
  next->stream = stream;
  // The decoder services its queue very quickly, so blocking on this write
  // should be fine. If we discover contention here, then adding more space for
  // items to next_stream_ should be fine too.
  xQueueSend(next_stream_, &next, portMAX_DELAY);
}

Decoder::Decoder(std::shared_ptr<SampleProcessor> processor)
    : processor_(processor), next_stream_(xQueueCreate(1, sizeof(void*))) {
  ESP_LOGI(kTag, "allocating codec buffer, %u KiB", kCodecBufferLength / 1024);
  codec_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kCodecBufferLength, sizeof(sample::Sample), MALLOC_CAP_DMA)),
      kCodecBufferLength};
}

/*
 * Main decoding loop. Handles watching for new streams, or continuing to nudge
 * along the current stream if we have one.
 */
IRAM_ATTR
void Decoder::Main() {
  for (;;) {
    // How long should we spend waiting for a command? By default, assume we're
    // idle and wait forever.
    TickType_t wait_time = portMAX_DELAY;
    if (!leftover_samples_.empty() || stream_) {
      // If we have work to do, then don't block waiting for a new stream.
      wait_time = 0;
    }

    NextStream* next;
    if (xQueueReceive(next_stream_, &next, wait_time)) {
      // Copy the data out of the queue, then clean up the item.
      std::shared_ptr<TaggedStream> new_stream = next->stream;
      delete next;

      // If we were already decoding, then make sure we finish up the current
      // file gracefully.
      if (stream_) {
        finishDecode(true);
      }

      // Ensure there's actually stream data; we might have been given nullptr
      // as a signal to stop.
      if (!new_stream) {
        continue;
      }

      // Start decoding the new stream.
      prepareDecode(new_stream);

      // Keep handling commands until the command queue is empty.
      continue;
    }

    // We should always have a stream if we returned from xQueueReceive without
    // receiving a new stream.
    assert(stream_);

    if (!continueDecode()) {
      finishDecode(false);
    }
  }
}

auto Decoder::prepareDecode(std::shared_ptr<TaggedStream> stream) -> void {
  auto stub_track = std::make_shared<TrackInfo>(TrackInfo{
      .tags = stream->tags(),
      .uri = stream->Filepath(),
      .duration = {},
      .start_offset = {},
      .bitrate_kbps = {},
      .encoding = stream->type(),
      .format = {},
  });

  codec_.reset(codecs::CreateCodecForType(stream->type()).value_or(nullptr));
  if (!codec_) {
    ESP_LOGE(kTag, "no codec found for stream");
    events::Audio().Dispatch(
        internal::DecodingFailedToStart{.track = stub_track});
    return;
  }

  auto open_res = codec_->OpenStream(stream, stream->Offset());
  if (open_res.has_error()) {
    ESP_LOGE(kTag, "codec failed to start: %s",
             codecs::ICodec::ErrorString(open_res.error()).c_str());
    events::Audio().Dispatch(
        internal::DecodingFailedToStart{.track = stub_track});
    return;
  }

  // Decoding started okay! Fill out the rest of the track info for this
  // stream.
  stream_ = stream;
  track_ = std::make_shared<TrackInfo>(TrackInfo{
      .tags = stream->tags(),
      .uri = stream->Filepath(),
      .duration = {},
      .start_offset = stream->Offset(),
      .bitrate_kbps = {},
      .encoding = stream->type(),
      .format =
          {
              .sample_rate = open_res->sample_rate_hz,
              .num_channels = open_res->num_channels,
              .bits_per_sample = 16,
          },
  });

  if (open_res->total_samples) {
    track_->duration = open_res->total_samples.value() /
                       open_res->num_channels / open_res->sample_rate_hz;
  }

  events::Audio().Dispatch(internal::DecodingStarted{.track = track_});
  processor_->beginStream(track_);
}

auto Decoder::continueDecode() -> bool {
  // First, see if we have any samples from a previous decode that still need
  // to be sent.
  if (!leftover_samples_.empty()) {
    leftover_samples_ = processor_->continueStream(leftover_samples_);
    return true;
  }

  // We might have already cleaned up the codec if the last decode pass of the
  // stream resulted in leftover samples.
  if (!codec_) {
    return false;
  }

  auto res = codec_->DecodeTo(codec_buffer_);
  if (res.has_error()) {
    return false;
  }

  if (res->samples_written > 0) {
    leftover_samples_ =
        processor_->continueStream(codec_buffer_.first(res->samples_written));
  }

  if (res->is_stream_finished) {
    // The codec has finished, so make sure we don't call it again.
    codec_.reset();
  }

  // We're done iff the codec has finished and we sent everything.
  return codec_ || !leftover_samples_.empty();
}

auto Decoder::finishDecode(bool cancel) -> void {
  assert(track_);

  // Tell everyone we're finished.
  if (cancel) {
    events::Audio().Dispatch(internal::DecodingCancelled{.track = track_});
  } else {
    events::Audio().Dispatch(internal::DecodingFinished{.track = track_});
  }
  processor_->endStream(cancel);

  // Clean up after ourselves.
  leftover_samples_ = {};
  stream_.reset();
  codec_.reset();
  track_.reset();
}

}  // namespace audio
