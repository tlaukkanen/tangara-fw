/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "sink_mixer.hpp"

#include <stdint.h>
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
static constexpr std::size_t kSampleBufferLength = 240 * 2 * sizeof(int32_t);

namespace audio {

SinkMixer::SinkMixer(StreamBufferHandle_t dest)
    : commands_(xQueueCreate(1, sizeof(Args))),
      is_idle_(xSemaphoreCreateBinary()),
      resampler_(nullptr),
      source_(xStreamBufferCreateWithCaps(kSourceBufferLength,
                                          1,
                                          MALLOC_CAP_SPIRAM)),
      sink_(dest) {
  input_stream_.reset(new RawStream(kSampleBufferLength));
  resampled_stream_.reset(new RawStream(kSampleBufferLength));

  // Pin to CORE0 because we need the FPU.
  // FIXME: A fixed point implementation could run freely on either core,
  // which should lead to a big performance increase.
  tasks::StartPersistent<tasks::Type::kMixer>(0, [&]() { Main(); });
}

SinkMixer::~SinkMixer() {
  vQueueDelete(commands_);
  vSemaphoreDelete(is_idle_);
  vStreamBufferDelete(source_);
}

auto SinkMixer::MixAndSend(InputStream& input, const StreamInfo::Pcm& target)
    -> std::size_t {
  if (input.info().format_as<StreamInfo::Pcm>() !=
      input_stream_->info().format_as<StreamInfo::Pcm>()) {
    xSemaphoreTake(is_idle_, portMAX_DELAY);
    Args args{
        .cmd = Command::kSetSourceFormat,
        .format = input.info().format_as<StreamInfo::Pcm>().value(),
    };
    xQueueSend(commands_, &args, portMAX_DELAY);
    xSemaphoreGive(is_idle_);
  }
  if (target_format_ != target) {
    xSemaphoreTake(is_idle_, portMAX_DELAY);
    Args args{
        .cmd = Command::kSetTargetFormat,
        .format = target,
    };
    xQueueSend(commands_, &args, portMAX_DELAY);
    xSemaphoreGive(is_idle_);
  }

  Args args{
      .cmd = Command::kReadBytes,
      .format = {},
  };
  xQueueSend(commands_, &args, portMAX_DELAY);

  auto buf = input.data();
  std::size_t bytes_sent =
      xStreamBufferSend(source_, buf.data(), buf.size_bytes(), portMAX_DELAY);
  input.consume(bytes_sent);
  return bytes_sent;
}

auto SinkMixer::Main() -> void {
  OutputStream input_receiver{input_stream_.get()};
  xSemaphoreGive(is_idle_);

  for (;;) {
    Args args;
    while (!xQueueReceive(commands_, &args, portMAX_DELAY)) {
    }
    switch (args.cmd) {
      case Command::kSetSourceFormat:
        ESP_LOGI(kTag, "setting source format");
        input_receiver.prepare(args.format, {});
        resampler_.reset();
        break;
      case Command::kSetTargetFormat:
        ESP_LOGI(kTag, "setting target format");
        target_format_ = args.format;
        resampler_.reset();
        break;
      case Command::kReadBytes:
        xSemaphoreTake(is_idle_, 0);
        while (!xStreamBufferIsEmpty(source_)) {
          auto buf = input_receiver.data();
          std::size_t bytes_received = xStreamBufferReceive(
              source_, buf.data(), buf.size_bytes(), portMAX_DELAY);
          input_receiver.add(bytes_received);
          HandleBytes();
        }
        xSemaphoreGive(is_idle_);
        break;
    }
  }
}

auto SinkMixer::HandleBytes() -> void {
  InputStream input{input_stream_.get()};
  auto pcm = input.info().format_as<StreamInfo::Pcm>();
  if (!pcm) {
    ESP_LOGE(kTag, "mixer got unsupported data");
    return;
  }

  if (*pcm == target_format_) {
    // The happiest possible case: the input format matches the output
    // format already. Streams like this should probably have bypassed the
    // mixer.
    // TODO(jacqueline): Make this an error; it's slow to use the mixer in this
    // case, compared to just writing directly to the sink.
    auto buf = input.data();
    std::size_t bytes_sent =
        xStreamBufferSend(sink_, buf.data(), buf.size_bytes(), portMAX_DELAY);
    input.consume(bytes_sent);
    return;
  }

  while (input_stream_->info().bytes_in_stream() >= sizeof(sample::Sample)) {
    RawStream* output_source;
    if (pcm->sample_rate != target_format_.sample_rate) {
      OutputStream resampled_writer{resampled_stream_.get()};
      if (Resample(input, resampled_writer)) {
        // Zero samples used or written. We need more input.
        break;
      }
      output_source = resampled_stream_.get();
    } else {
      output_source = input_stream_.get();
    }

    size_t bytes_consumed = output_source->info().bytes_in_stream();
    size_t bytes_to_send = output_source->info().bytes_in_stream();

    if (target_format_.bits_per_sample == 16) {
      // This is slightly scary; we're basically reaching into the internals of
      // the stream buffer to do in-place conversion of samples. Saving an
      // extra buffer + copy into that buffer is certainly worth it however.
      cpp::span<sample::Sample> src =
          output_source->data_as<sample::Sample>().first(
              output_source->info().bytes_in_stream() / sizeof(sample::Sample));
      cpp::span<int16_t> dest{reinterpret_cast<int16_t*>(src.data()),
                              src.size()};

      ApplyDither(src, 16);
      Downscale(src, dest);

      bytes_consumed = src.size_bytes();
      bytes_to_send = src.size_bytes() / 2;
    }

    InputStream output{output_source};
    cpp::span<const std::byte> buf = output.data();

    size_t bytes_sent = 0;
    while (bytes_sent < bytes_to_send) {
      auto cropped = buf.subspan(bytes_sent, bytes_to_send - bytes_sent);
      bytes_sent += xStreamBufferSend(sink_, cropped.data(),
                                      cropped.size_bytes(), portMAX_DELAY);
    }
    output.consume(bytes_consumed);
  }
}

auto SinkMixer::Resample(InputStream& in, OutputStream& out) -> bool {
  if (resampler_ == nullptr) {
    ESP_LOGI(kTag, "creating new resampler");
    auto format = in.info().format_as<StreamInfo::Pcm>();
    resampler_.reset(new Resampler(
        format->sample_rate, target_format_.sample_rate, format->channels));
  }

  auto res = resampler_->Process(in.data_as<sample::Sample>(),
                                 out.data_as<sample::Sample>(), false);

  in.consume(res.first * sizeof(sample::Sample));
  out.add(res.second * sizeof(sample::Sample));

  return res.first == 0 && res.second == 0;
}

auto SinkMixer::Downscale(cpp::span<sample::Sample> samples,
                          cpp::span<int16_t> output) -> void {
  for (size_t i = 0; i < samples.size(); i++) {
    output[i] = sample::ToSigned16Bit(samples[i]);
  }
}

auto SinkMixer::ApplyDither(cpp::span<sample::Sample> samples,
                            uint_fast8_t bits) -> void {
  static uint32_t prnd;
  for (auto& s : samples) {
    prnd = (prnd * 0x19660dL + 0x3c6ef35fL) & 0xffffffffL;
    s = sample::Clip(
        static_cast<int64_t>(s) +
        (static_cast<int>(prnd) >> (sizeof(sample::Sample) - bits)));
  }
}

}  // namespace audio
