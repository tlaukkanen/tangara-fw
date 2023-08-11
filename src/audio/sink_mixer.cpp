/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "sink_mixer.hpp"

#include <stdint.h>
#include <algorithm>
#include <cmath>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "idf_additions.h"
#include "resample.hpp"
#include "sample.hpp"

#include "stream_info.hpp"
#include "tasks.hpp"

static constexpr char kTag[] = "mixer";

static constexpr std::size_t kSourceBufferLength = 8 * 1024;
static constexpr std::size_t kSampleBufferLength = 240 * 2;

namespace audio {

SinkMixer::SinkMixer(IAudioSink* sink)
    : commands_(xQueueCreate(1, sizeof(Args))),
      resampler_(nullptr),
      source_(xStreamBufferCreateWithCaps(kSourceBufferLength,
                                          1,
                                          MALLOC_CAP_SPIRAM)),
      sink_(sink) {
  input_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kSampleBufferLength, sizeof(sample::Sample), MALLOC_CAP_SPIRAM)),
      kSampleBufferLength};
  input_buffer_as_bytes_ = {reinterpret_cast<std::byte*>(input_buffer_.data()),
                            input_buffer_.size_bytes()};

  resampled_buffer_ = {
      reinterpret_cast<sample::Sample*>(heap_caps_calloc(
          kSampleBufferLength, sizeof(sample::Sample), MALLOC_CAP_SPIRAM)),
      kSampleBufferLength};

  tasks::StartPersistent<tasks::Type::kMixer>([&]() { Main(); });
}

SinkMixer::~SinkMixer() {
  vQueueDelete(commands_);
  vStreamBufferDelete(source_);
}

auto SinkMixer::MixAndSend(cpp::span<sample::Sample> input,
                           const IAudioSink::Format& format,
                           bool is_eos) -> void {
  Args args{
      .format = format,
      .samples_available = input.size(),
      .is_end_of_stream = is_eos,
  };
  xQueueSend(commands_, &args, portMAX_DELAY);

  cpp::span<std::byte> input_as_bytes = {
      reinterpret_cast<std::byte*>(input.data()), input.size_bytes()};
  size_t bytes_sent = 0;
  while (bytes_sent < input_as_bytes.size()) {
    bytes_sent +=
        xStreamBufferSend(source_, input_as_bytes.subspan(bytes_sent).data(),
                          input_as_bytes.size() - bytes_sent, portMAX_DELAY);
  }
}

auto SinkMixer::Main() -> void {
  for (;;) {
    Args args;
    while (!xQueueReceive(commands_, &args, portMAX_DELAY)) {
    }
    if (args.format != source_format_) {
      resampler_.reset();
      source_format_ = args.format;
      leftover_bytes_ = 0;
      leftover_offset_ = 0;

      auto new_target = sink_->PrepareFormat(args.format);
      if (new_target != target_format_) {
        // The new format is different to the old one. Wait for the sink to
        // drain before continuing.
        while (!xStreamBufferIsEmpty(sink_->stream())) {
          ESP_LOGI(kTag, "waiting for sink stream to drain...");
          // TODO(jacqueline): Get the sink drain ISR to notify us of this
          // via semaphore instead of busy-ish waiting.
          vTaskDelay(pdMS_TO_TICKS(10));
        }

        sink_->Configure(new_target);
      }
      target_format_ = new_target;
    }

    // Loop until we finish reading all the bytes indicated. There might be
    // leftovers from each iteration, and from this process as a whole,
    // depending on the resampling stage.
    size_t bytes_read = 0;
    size_t bytes_to_read = args.samples_available * sizeof(sample::Sample);
    while (bytes_read < bytes_to_read) {
      // First top up the input buffer, taking care not to overwrite anything
      // remaining from a previous iteration.
      size_t bytes_read_this_it = xStreamBufferReceive(
          source_,
          input_buffer_as_bytes_.subspan(leftover_offset_ + leftover_bytes_)
              .data(),
          std::min(input_buffer_as_bytes_.size() - leftover_offset_ -
                       leftover_bytes_,
                   bytes_to_read - bytes_read),
          portMAX_DELAY);
      bytes_read += bytes_read_this_it;

      // Calculate the number of whole samples that are now in the input buffer.
      size_t bytes_in_buffer = bytes_read_this_it + leftover_bytes_;
      size_t samples_in_buffer = bytes_in_buffer / sizeof(sample::Sample);

      size_t samples_used = HandleSamples(
          input_buffer_.subspan(leftover_offset_).first(samples_in_buffer),
          args.is_end_of_stream && bytes_read == bytes_to_read);

      // Maybe the resampler didn't consume everything. Maybe the last few
      // bytes we read were half a frame. Either way, we need to calculate the
      // size of the remainder in bytes.
      size_t bytes_used = samples_used * sizeof(sample::Sample);
      assert(bytes_used <= bytes_in_buffer);
      leftover_bytes_ = bytes_in_buffer - bytes_used;
      if (leftover_bytes_ == 0) {
        leftover_offset_ = 0;
      } else {
        leftover_offset_ += bytes_used;
      }
    }
  }
}

auto SinkMixer::HandleSamples(cpp::span<sample::Sample> input, bool is_eos)
    -> size_t {
  if (source_format_ == target_format_) {
    // The happiest possible case: the input format matches the output
    // format already.
    std::size_t bytes_sent = xStreamBufferSend(
        sink_->stream(), input.data(), input.size_bytes(), portMAX_DELAY);
    return bytes_sent / sizeof(sample::Sample);
  }

  size_t samples_used = 0;
  while (samples_used < input.size()) {
    cpp::span<sample::Sample> output_source;
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
                                                    resampled_buffer_, is_eos);
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

    size_t bytes_sent = 0;
    size_t bytes_to_send = output_source.size_bytes();
    while (bytes_sent < bytes_to_send) {
      bytes_sent += xStreamBufferSend(
          sink_->stream(),
          reinterpret_cast<std::byte*>(output_source.data()) + bytes_sent,
          bytes_to_send - bytes_sent, portMAX_DELAY);
    }
  }
  return samples_used;
}

}  // namespace audio
