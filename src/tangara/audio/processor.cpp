/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/processor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include "assert.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "audio/i2s_audio_output.hpp"
#include "audio/resample.hpp"
#include "drivers/i2s_dac.hpp"
#include "drivers/pcm_buffer.hpp"
#include "events/event_queue.hpp"
#include "sample.hpp"
#include "tasks.hpp"

[[maybe_unused]] static constexpr char kTag[] = "mixer";

static const size_t kSampleBufferLength = drivers::kI2SBufferLengthFrames * 2;
static const size_t kSourceBufferLength = kSampleBufferLength * 2;

namespace audio {

/*
 * The output format to convert all sources to. This is currently fixed because
 * the Bluetooth output doesn't support runtime configuration of its input
 * format.
 */
static const I2SAudioOutput::Format kTargetFormat{
    .sample_rate = 48000,
    .num_channels = 2,
    .bits_per_sample = 16,
};

SampleProcessor::SampleProcessor(drivers::PcmBuffer& sink)
    : commands_(xQueueCreate(2, sizeof(Args))),
      source_(xStreamBufferCreateWithCaps(kSourceBufferLength + 1,
                                          sizeof(sample::Sample),
                                          MALLOC_CAP_DMA)),
      sink_(sink),
      unprocessed_samples_(0) {
  tasks::StartPersistent<tasks::Type::kAudioConverter>([&]() { Main(); });
}

SampleProcessor::~SampleProcessor() {
  vQueueDelete(commands_);
  vStreamBufferDeleteWithCaps(source_);
}

auto SampleProcessor::SetOutput(std::shared_ptr<IAudioOutput> output) -> void {
  // Make sure our fixed output format is valid.
  assert(output->PrepareFormat(kTargetFormat) == kTargetFormat);
  output->Configure(kTargetFormat);

  // FIXME: We should add synchronisation here, but we should be careful
  // about not impacting performance given that the output will change only
  // very rarely (if ever).
  output_ = output;
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

auto SampleProcessor::continueStream(std::span<sample::Sample> input)
    -> std::span<sample::Sample> {
  size_t bytes_sent = xStreamBufferSend(source_, input.data(),
                                        input.size_bytes(), pdMS_TO_TICKS(100));
  if (!bytes_sent) {
    // If nothing could be sent, then bail out early. We don't want to send a
    // samples_available command with zero samples.
    return input;
  }

  // We should only ever be placing whole samples into the buffer. If half
  // samples start being sent, then this indicates a serious bug somewhere.
  size_t samples_sent = bytes_sent / sizeof(sample::Sample);
  assert(samples_sent * sizeof(sample::Sample) == bytes_sent);

  Args args{
      .track = nullptr,
      .samples_available = samples_sent,
      .is_end_of_stream = false,
      .clear_buffers = false,
  };
  xQueueSend(commands_, &args, portMAX_DELAY);

  return input.subspan(samples_sent);
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

IRAM_ATTR
auto SampleProcessor::Main() -> void {
  for (;;) {
    // Block indefinitely if the processor is idle. Otherwise check briefly for
    // new commands, then continue processing.
    TickType_t wait = hasPendingWork() ? 0 : portMAX_DELAY;

    Args args;
    if (xQueueReceive(commands_, &args, wait)) {
      if (args.is_end_of_stream && args.clear_buffers) {
        // The new command is telling us to clear our buffers! This includes
        // discarding any commands that have backed up without being processed.
        // Discard all the old commands, then immediately handle the end of
        // stream.
        while (!pending_commands_.empty()) {
          Args discard = pending_commands_.front();
          pending_commands_.pop_front();
          discardCommand(discard);
        }
        handleEndStream(true);
      } else {
        pending_commands_.push_back(args);
      }
    }

    // We need to finish flushing all processed samples before we can process
    // more samples.
    if (!output_buffer_.isEmpty() && flushOutputBuffer()) {
      continue;
    }

    // We need to finish processing all the samples we've been told about
    // before we handle backed up commands.
    if (unprocessed_samples_ && !processSamples(false)) {
      continue;
    }

    while (!pending_commands_.empty()) {
      args = pending_commands_.front();
      pending_commands_.pop_front();

      if (args.track) {
        handleBeginStream(*args.track);
        delete args.track;
      }
      if (args.samples_available) {
        unprocessed_samples_ += args.samples_available;
      }
      if (args.is_end_of_stream) {
        if (processSamples(true) || args.clear_buffers) {
          handleEndStream(args.clear_buffers);
        } else {
          // The output filled up while we were trying to flush the last
          // samples of this stream, and we haven't been told to clear our
          // buffers. Retry handling this command later.
          pending_commands_.push_front(args);
          break;
        }
      }
    }
  }
}

auto SampleProcessor::handleBeginStream(std::shared_ptr<TrackInfo> track)
    -> void {
  // If the new stream's sample rate doesn't match our canonical sample rate,
  // then prepare to start resampling.
  if (track->format.sample_rate != kTargetFormat.sample_rate) {
    ESP_LOGI(kTag, "resampling %lu -> %lu", track->format.sample_rate,
             kTargetFormat.sample_rate);
    if (!resampler_ || resampler_->sourceRate() != track->format.sample_rate) {
      // If there's already a resampler instance for this source rate, then
      // reuse it to help gapless playback work smoothly.
      resampler_.reset(new Resampler(track->format.sample_rate,
                                     kTargetFormat.sample_rate,
                                     track->format.num_channels));
    }
  } else {
    resampler_.reset();
  }

  // If the new stream has only one channel, then we double it to get stereo
  // audio.
  // FIXME: If the Bluetooth stack allowed us to configure the number of
  // channels, we could remove this.
  double_samples_ = track->format.num_channels != kTargetFormat.num_channels;

  events::Audio().Dispatch(internal::StreamStarted{
      .track = track,
      .sink_format = kTargetFormat,
      .cue_at_sample = sink_.totalSent(),
  });
}

IRAM_ATTR
auto SampleProcessor::processSamples(bool finalise) -> bool {
  for (;;) {
    bool out_of_work = true;

    // First, fill up our input buffer with samples.
    if (unprocessed_samples_ > 0) {
      out_of_work = false;
      auto input = input_buffer_.writeAcquire();

      size_t bytes_received = xStreamBufferReceive(
          source_, input.data(),
          std::min(input.size_bytes(),
                   unprocessed_samples_ * sizeof(sample::Sample)),
          0);

      // We should never receive a half sample. Blow up immediately if we do.
      size_t samples_received = bytes_received / sizeof(sample::Sample);
      assert(samples_received * sizeof(sample::Sample) == bytes_received);

      unprocessed_samples_ -= samples_received;
      input_buffer_.writeCommit(samples_received);
    }

    // Next, push input samples through the resampler. In the best case, this
    // is a simple copy operation.
    if (!input_buffer_.isEmpty()) {
      out_of_work = false;
      auto resample_input = input_buffer_.readAcquire();
      auto resample_output = resampled_buffer_.writeAcquire();

      size_t read, wrote;
      if (resampler_) {
        std::tie(read, wrote) =
            resampler_->Process(resample_input, resample_output, finalise);
      } else {
        read = wrote = std::min(resample_input.size(), resample_output.size());
        std::copy_n(resample_input.begin(), read, resample_output.begin());
      }

      input_buffer_.readCommit(read);
      resampled_buffer_.writeCommit(wrote);
    }

    // Next, we need to make sure the output is in stereo. This is also a simple
    // copy in the best case.
    if (!resampled_buffer_.isEmpty()) {
      out_of_work = false;
      auto channels_input = resampled_buffer_.readAcquire();
      auto channels_output = output_buffer_.writeAcquire();
      size_t read, wrote;
      if (double_samples_) {
        wrote = channels_output.size();
        read = wrote / 2;
        if (read > channels_input.size()) {
          read = channels_input.size();
          wrote = read * 2;
        }
        for (size_t i = 0; i < read; i++) {
          channels_output[i * 2] = channels_input[i];
          channels_output[(i * 2) + 1] = channels_input[i];
        }
      } else {
        read = wrote = std::min(channels_input.size(), channels_output.size());
        std::copy_n(channels_input.begin(), read, channels_output.begin());
      }
      resampled_buffer_.readCommit(read);
      output_buffer_.writeCommit(wrote);
    }

    // Finally, flush whatever ended up in the output buffer.
    if (flushOutputBuffer()) {
      if (out_of_work) {
        return true;
      }
    } else {
      // The output is congested. Back off of processing for a moment.
      return false;
    }
  }
}

auto SampleProcessor::handleEndStream(bool clear_bufs) -> void {
  if (clear_bufs) {
    sink_.clear();

    input_buffer_.clear();
    resampled_buffer_.clear();
    output_buffer_.clear();

    size_t bytes_discarded = 0;
    size_t bytes_to_discard = unprocessed_samples_ * sizeof(sample::Sample);
    auto scratch_buf = output_buffer_.writeAcquire();
    while (bytes_discarded < bytes_to_discard) {
      size_t bytes_read =
          xStreamBufferReceive(source_, scratch_buf.data(),
                               std::min(scratch_buf.size_bytes(),
                                        bytes_to_discard - bytes_discarded),
                               0);
      bytes_discarded += bytes_read;
    }
    unprocessed_samples_ = 0;
  }

  events::Audio().Dispatch(internal::StreamEnded{
      .cue_at_sample = sink_.totalSent(),
  });
}

auto SampleProcessor::hasPendingWork() -> bool {
  return !pending_commands_.empty() || unprocessed_samples_ > 0 ||
         !input_buffer_.isEmpty() || !resampled_buffer_.isEmpty() ||
         !output_buffer_.isEmpty();
}

IRAM_ATTR
auto SampleProcessor::flushOutputBuffer() -> bool {
  auto samples = output_buffer_.readAcquire();
  size_t sent = sink_.send(samples);
  output_buffer_.readCommit(sent);
  return output_buffer_.isEmpty();
}

auto SampleProcessor::discardCommand(Args& command) -> void {
  if (command.track) {
    delete command.track;
  }
  if (command.samples_available) {
    unprocessed_samples_ += command.samples_available;
  }
  // End of stream commands can just be dropped without further action.
}

SampleProcessor::Buffer::Buffer()
    : buffer_(reinterpret_cast<sample::Sample*>(
                  heap_caps_calloc(kSampleBufferLength,
                                   sizeof(sample::Sample),
                                   MALLOC_CAP_DMA)),
              kSampleBufferLength),
      samples_in_buffer_() {}

SampleProcessor::Buffer::~Buffer() {
  heap_caps_free(buffer_.data());
}

auto SampleProcessor::Buffer::writeAcquire() -> std::span<sample::Sample> {
  return buffer_.subspan(samples_in_buffer_.size());
}

auto SampleProcessor::Buffer::writeCommit(size_t samples) -> void {
  if (samples == 0) {
    return;
  }
  samples_in_buffer_ = buffer_.first(samples + samples_in_buffer_.size());
}

auto SampleProcessor::Buffer::readAcquire() -> std::span<sample::Sample> {
  return samples_in_buffer_;
}

auto SampleProcessor::Buffer::readCommit(size_t samples) -> void {
  if (samples == 0) {
    return;
  }
  samples_in_buffer_ = samples_in_buffer_.subspan(samples);

  // Move the leftover samples to the front of the buffer, so that we're setup
  // for a new write.
  if (!samples_in_buffer_.empty()) {
    std::memmove(buffer_.data(), samples_in_buffer_.data(),
                 samples_in_buffer_.size_bytes());
    samples_in_buffer_ = buffer_.first(samples_in_buffer_.size());
  }
}

auto SampleProcessor::Buffer::isEmpty() -> bool {
  return samples_in_buffer_.empty();
}

auto SampleProcessor::Buffer::clear() -> void {
  samples_in_buffer_ = {};
}

}  // namespace audio
