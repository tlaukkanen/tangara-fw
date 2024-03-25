/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_converter.hpp"
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "audio_events.hpp"
#include "audio_sink.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "i2s_dac.hpp"
#include "idf_additions.h"

#include "resample.hpp"
#include "sample.hpp"
#include "tasks.hpp"

[[maybe_unused]] static constexpr char kTag[] = "mixer";

static constexpr std::size_t kSampleBufferLength =
    drivers::kI2SBufferLengthFrames * sizeof(sample::Sample);
static constexpr std::size_t kSourceBufferLength = kSampleBufferLength * 2;

namespace audio {

SampleConverter::SampleConverter()
    : commands_(xQueueCreate(1, sizeof(Args))),
      resampler_(nullptr),
      source_(xStreamBufferCreateWithCaps(kSourceBufferLength,
                                          sizeof(sample::Sample) * 2,
                                          MALLOC_CAP_DMA)),
      leftover_bytes_(0),
      samples_sunk_(0) {
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

SampleConverter::~SampleConverter() {
  vQueueDelete(commands_);
  vStreamBufferDelete(source_);
}

auto SampleConverter::SetOutput(std::shared_ptr<IAudioOutput> output) -> void {
  // FIXME: We should add synchronisation here, but we should be careful about
  // not impacting performance given that the output will change only very
  // rarely (if ever).
  sink_ = output;
}

auto SampleConverter::ConvertSamples(cpp::span<sample::Sample> input,
                                     const IAudioOutput::Format& format,
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
    bytes_sent += xStreamBufferSend(
        source_, input_as_bytes.subspan(bytes_sent).data(),
        input_as_bytes.size() - bytes_sent, pdMS_TO_TICKS(100));
  }
}

auto SampleConverter::Main() -> void {
  for (;;) {
    Args args;
    while (!xQueueReceive(commands_, &args, portMAX_DELAY)) {
    }
    if (args.format != source_format_) {
      resampler_.reset();
      source_format_ = args.format;
      leftover_bytes_ = 0;

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

      // Send a final sample count for the previous sample rate.
      if (samples_sunk_ > 0) {
        events::Audio().Dispatch(internal::ConverterProgress{
            .samples_sunk = samples_sunk_,
        });
      }

      samples_sunk_ = 0;
      events::Audio().Dispatch(internal::ConverterConfigurationChanged{
          .src_format = source_format_,
          .dst_format = target_format_,
      });
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
          source_, input_buffer_as_bytes_.subspan(leftover_bytes_).data(),
          std::min(input_buffer_as_bytes_.size() - leftover_bytes_,
                   bytes_to_read - bytes_read),
          portMAX_DELAY);
      bytes_read += bytes_read_this_it;

      // Calculate the number of whole samples that are now in the input buffer.
      size_t bytes_in_buffer = bytes_read_this_it + leftover_bytes_;
      size_t samples_in_buffer = bytes_in_buffer / sizeof(sample::Sample);

      size_t samples_used =
          HandleSamples(input_buffer_.first(samples_in_buffer),
                        args.is_end_of_stream && bytes_read == bytes_to_read);

      // Maybe the resampler didn't consume everything. Maybe the last few
      // bytes we read were half a frame. Either way, we need to calculate the
      // size of the remainder in bytes, then move it to the front of our
      // buffer.
      size_t bytes_used = samples_used * sizeof(sample::Sample);
      assert(bytes_used <= bytes_in_buffer);

      leftover_bytes_ = bytes_in_buffer - bytes_used;
      if (leftover_bytes_ > 0) {
        std::memmove(input_buffer_as_bytes_.data(),
                     input_buffer_as_bytes_.data() + bytes_used,
                     leftover_bytes_);
      }
    }
  }
}

auto SampleConverter::HandleSamples(cpp::span<sample::Sample> input,
                                    bool is_eos) -> size_t {
  if (source_format_ == target_format_) {
    // The happiest possible case: the input format matches the output
    // format already.
    SendToSink(input);
    return input.size();
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

    SendToSink(output_source);
  }
  return samples_used;
}

auto SampleConverter::SendToSink(cpp::span<sample::Sample> samples) -> void {
  // Update the number of samples sunk so far *before* actually sinking them,
  // since writing to the stream buffer will block when the buffer gets full.
  samples_sunk_ += samples.size();
  if (samples_sunk_ >=
      target_format_.sample_rate * target_format_.num_channels) {
    events::Audio().Dispatch(internal::ConverterProgress{
        .samples_sunk = samples_sunk_,
    });
    samples_sunk_ = 0;
  }

  xStreamBufferSend(sink_->stream(),
                    reinterpret_cast<std::byte*>(samples.data()),
                    samples.size_bytes(), portMAX_DELAY);
}

}  // namespace audio
