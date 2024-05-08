/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/processor.hpp"
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "drivers/i2s_dac.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "events/event_queue.hpp"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "audio/resample.hpp"
#include "sample.hpp"
#include "tasks.hpp"

[[maybe_unused]] static constexpr char kTag[] = "mixer";

static constexpr std::size_t kSampleBufferLength =
    drivers::kI2SBufferLengthFrames * sizeof(sample::Sample) * 2;
static constexpr std::size_t kSourceBufferLength = kSampleBufferLength * 2;

namespace audio {

SampleProcessor::SampleProcessor(StreamBufferHandle_t sink)
    : commands_(xQueueCreate(1, sizeof(Args))),
      resampler_(nullptr),
      source_(xStreamBufferCreateWithCaps(kSourceBufferLength,
                                          sizeof(sample::Sample) * 2,
                                          MALLOC_CAP_DMA)),
      sink_(sink),
      leftover_bytes_(0),
      samples_written_(0) {
  input_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kSampleBufferLength, sizeof(sample::Sample), MALLOC_CAP_DMA)),
      kSampleBufferLength};
  input_buffer_as_bytes_ = {reinterpret_cast<std::byte*>(input_buffer_.data()),
                            input_buffer_.size_bytes()};

  resampled_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kSampleBufferLength, sizeof(sample::Sample), MALLOC_CAP_DMA)),
      kSampleBufferLength};

  tasks::StartPersistent<tasks::Type::kAudioConverter>([&]() { Main(); });
}

SampleProcessor::~SampleProcessor() {
  vQueueDelete(commands_);
  vStreamBufferDelete(source_);
}

auto SampleProcessor::SetOutput(std::shared_ptr<IAudioOutput> output) -> void {
  assert(xStreamBufferIsEmpty(sink_));
  // FIXME: We should add synchronisation here, but we should be careful
  // about not impacting performance given that the output will change only
  // very rarely (if ever).
  output_ = output;
  samples_written_ = output_->samplesUsed();
}

auto SampleProcessor::beginStream(std::shared_ptr<TrackInfo> track) -> void {
  Args args{
      .track = new std::shared_ptr<TrackInfo>(track),
      .samples_available = 0,
      .is_end_of_stream = false,
      .clear_buffers = false,
  };
  xQueueSend(commands_, &args, portMAX_DELAY);
}

auto SampleProcessor::continueStream(std::span<sample::Sample> input) -> void {
  Args args{
      .track = nullptr,
      .samples_available = input.size(),
      .is_end_of_stream = false,
      .clear_buffers = false,
  };
  xQueueSend(commands_, &args, portMAX_DELAY);
  xStreamBufferSend(source_, input.data(), input.size_bytes(), portMAX_DELAY);
}

auto SampleProcessor::endStream(bool cancelled) -> void {
  Args args{
      .track = nullptr,
      .samples_available = 0,
      .is_end_of_stream = true,
      .clear_buffers = cancelled,
  };
  xQueueSend(commands_, &args, portMAX_DELAY);
}

auto SampleProcessor::Main() -> void {
  for (;;) {
    Args args;
    while (!xQueueReceive(commands_, &args, portMAX_DELAY)) {
    }

    if (args.track) {
      handleBeginStream(*args.track);
      delete args.track;
    }
    if (args.samples_available) {
      handleContinueStream(args.samples_available);
    }
    if (args.is_end_of_stream) {
      handleEndStream(args.clear_buffers);
    }
  }
}

auto SampleProcessor::handleBeginStream(std::shared_ptr<TrackInfo> track)
    -> void {
  if (track->format != source_format_) {
    source_format_ = track->format;
    // The new stream has a different format to the previous stream (or there
    // was no previous stream).
    // First, clean up our filters.
    resampler_.reset();
    leftover_bytes_ = 0;

    // If the output is idle, then we can reconfigure it to the closest format
    // to our new source.
    // If the output *wasn't* idle, then we can't reconfigure without an
    // audible gap in playback. So instead, we simply keep the same target
    // format and begin resampling.
    if (xStreamBufferIsEmpty(sink_)) {
      target_format_ = output_->PrepareFormat(track->format);
      output_->Configure(target_format_);
    }
  }

  if (xStreamBufferIsEmpty(sink_)) {
    samples_written_ = output_->samplesUsed();
  }

  events::Audio().Dispatch(internal::StreamStarted{
      .track = track,
      .sink_format = target_format_,
      .cue_at_sample = samples_written_,
  });
}

auto SampleProcessor::handleContinueStream(size_t samples_available) -> void {
  // Loop until we finish reading all the bytes indicated. There might be
  // leftovers from each iteration, and from this process as a whole,
  // depending on the resampling stage.
  size_t bytes_read = 0;
  size_t bytes_to_read = samples_available * sizeof(sample::Sample);
  while (bytes_read < bytes_to_read) {
    // First top up the input buffer, taking care not to overwrite anything
    // remaining from a previous iteration.
    size_t bytes_read_this_it = xStreamBufferReceive(
        source_, input_buffer_as_bytes_.subspan(leftover_bytes_).data(),
        std::min(input_buffer_as_bytes_.size() - leftover_bytes_,
                 bytes_to_read - bytes_read),
        portMAX_DELAY);
    bytes_read += bytes_read_this_it;

    // Calculate the number of whole samples that are now in the input buffer.
    size_t bytes_in_buffer = bytes_read_this_it + leftover_bytes_;
    size_t samples_in_buffer = bytes_in_buffer / sizeof(sample::Sample);

    size_t samples_used = handleSamples(input_buffer_.first(samples_in_buffer));

    // Maybe the resampler didn't consume everything. Maybe the last few
    // bytes we read were half a frame. Either way, we need to calculate the
    // size of the remainder in bytes, then move it to the front of our
    // buffer.
    size_t bytes_used = samples_used * sizeof(sample::Sample);
    assert(bytes_used <= bytes_in_buffer);

    leftover_bytes_ = bytes_in_buffer - bytes_used;
    if (leftover_bytes_ > 0) {
      std::memmove(input_buffer_as_bytes_.data(),
                   input_buffer_as_bytes_.data() + bytes_used, leftover_bytes_);
    }
  }
}

auto SampleProcessor::handleSamples(std::span<sample::Sample> input) -> size_t {
  if (source_format_ == target_format_) {
    // The happiest possible case: the input format matches the output
    // format already.
    sendToSink(input);
    return input.size();
  }

  size_t samples_used = 0;
  while (samples_used < input.size()) {
    std::span<sample::Sample> output_source;
    if (source_format_.sample_rate != target_format_.sample_rate) {
      if (resampler_ == nullptr) {
        ESP_LOGI(kTag, "creating new resampler for %lu -> %lu",
                 source_format_.sample_rate, target_format_.sample_rate);
        resampler_.reset(new Resampler(source_format_.sample_rate,
                                       target_format_.sample_rate,
                                       source_format_.num_channels));
      }

      size_t read, written;
      std::tie(read, written) = resampler_->Process(input.subspan(samples_used),
                                                    resampled_buffer_, false);
      samples_used += read;

      if (read == 0 && written == 0) {
        // Zero samples used or written. We need more input.
        break;
      }
      output_source = resampled_buffer_.first(written);
    } else {
      output_source = input;
      samples_used = input.size();
    }

    sendToSink(output_source);
  }

  return samples_used;
}

auto SampleProcessor::handleEndStream(bool clear_bufs) -> void {
  if (resampler_ && !clear_bufs) {
    size_t read, written;
    std::tie(read, written) = resampler_->Process({}, resampled_buffer_, true);

    if (written > 0) {
      sendToSink(resampled_buffer_.first(written));
    }
  }

  if (clear_bufs) {
    assert(xStreamBufferReset(sink_));
    samples_written_ = output_->samplesUsed();
  }

  // FIXME: This discards any leftover samples, but there probably shouldn't be
  // any leftover samples. Can this be an assert instead?
  leftover_bytes_ = 0;

  events::Audio().Dispatch(internal::StreamEnded{
      .cue_at_sample = samples_written_,
  });
}

auto SampleProcessor::sendToSink(std::span<sample::Sample> samples) -> void {
  auto data = std::as_bytes(samples);
  xStreamBufferSend(sink_, data.data(), data.size(), portMAX_DELAY);

  uint32_t samples_before_overflow =
      std::numeric_limits<uint32_t>::max() - samples_written_;
  if (samples_before_overflow < samples.size()) {
    samples_written_ = samples.size() - samples_before_overflow;
  } else {
    samples_written_ += samples.size();
  }
}

}  // namespace audio
