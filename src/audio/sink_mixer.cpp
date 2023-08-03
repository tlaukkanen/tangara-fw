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
#include "samplerate.h"

#include "stream_info.hpp"
#include "tasks.hpp"

static constexpr char kTag[] = "mixer";

static constexpr std::size_t kSourceBufferLength = 2 * 1024;
static constexpr std::size_t kInputBufferLength = 2 * 1024;
static constexpr std::size_t kReformatBufferLength = 8 * 1024;
static constexpr std::size_t kResampleBufferLength = kReformatBufferLength;
static constexpr std::size_t kQuantisedBufferLength = 1 * 1024;

namespace audio {

SinkMixer::SinkMixer(StreamBufferHandle_t dest)
    : commands_(xQueueCreate(1, sizeof(Args))),
      is_idle_(xSemaphoreCreateBinary()),
      resampler_(nullptr),
      source_(xStreamBufferCreate(kSourceBufferLength, 1)),
      sink_(dest) {
  input_stream_.reset(new RawStream(kInputBufferLength));
  floating_point_stream_.reset(new RawStream(kReformatBufferLength, MALLOC_CAP_SPIRAM));
  resampled_stream_.reset(new RawStream(kResampleBufferLength, MALLOC_CAP_SPIRAM));

  quantisation_buffer_ = {
      reinterpret_cast<std::byte*>(heap_caps_malloc(
          kQuantisedBufferLength, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      kQuantisedBufferLength};
  quantisation_buffer_as_ints_ = {
      reinterpret_cast<int*>(quantisation_buffer_.data()),
      quantisation_buffer_.size_bytes() / 4};
  quantisation_buffer_as_shorts_ = {
      reinterpret_cast<short*>(quantisation_buffer_.data()),
      quantisation_buffer_.size_bytes() / 2};

  tasks::StartPersistent<tasks::Type::kMixer>([&]() { Main(); });
}

SinkMixer::~SinkMixer() {
  vQueueDelete(commands_);
  vSemaphoreDelete(is_idle_);
  vStreamBufferDelete(source_);
  heap_caps_free(quantisation_buffer_.data());
  if (resampler_ != nullptr) {
    src_delete(resampler_);
  }
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
        break;
      case Command::kSetTargetFormat:
        ESP_LOGI(kTag, "setting target format");
        target_format_ = args.format;
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

  // Work out the resampling ratio using floating point arithmetic, since
  // relying on the FPU for this will be much faster, and the difference in
  // accuracy is unlikely to be noticeable.
  float src_ratio = static_cast<float>(target_format_.sample_rate) /
                    static_cast<float>(pcm->sample_rate);

  // Loop until we don't have any complete frames left in the input stream,
  // where a 'frame' is one complete sample per channel.
  while (!input_stream_->empty()) {
    // The first step of both resampling and requantising is to convert the
    // fixed point pcm input data into 32 bit floating point samples.
    OutputStream floating_writer{floating_point_stream_.get()};
    if (pcm->bits_per_sample == 16) {
      ConvertFixedToFloating<short>(input, floating_writer);
    } else {
      // FIXME: We should consider treating 24 bit and 32 bit samples
      // differently.
      ConvertFixedToFloating<int>(input, floating_writer);
    }

    InputStream floating_reader{floating_point_stream_.get()};

    while (!floating_point_stream_->empty()) {
      RawStream* quantisation_source;
      if (pcm->sample_rate != target_format_.sample_rate) {
        // The input data needs to be resampled before being sent to the sink.
        OutputStream resample_writer{resampled_stream_.get()};
        Resample(src_ratio, pcm->channels, floating_reader, resample_writer);
        quantisation_source = resampled_stream_.get();
      } else {
        // The input data already has an acceptable sample rate. All we need to
        // do is quantise it.
        quantisation_source = floating_point_stream_.get();
      }

      InputStream quantise_reader{quantisation_source};
      while (!quantisation_source->empty()) {
        std::size_t samples_available;
        if (target_format_.bits_per_sample == 16) {
          samples_available = Quantise<short>(quantise_reader);
        } else {
          samples_available = Quantise<int>(quantise_reader);
        }

        assert(samples_available * target_format_.real_bytes_per_sample() <=
               quantisation_buffer_.size_bytes());

        std::size_t bytes_sent = xStreamBufferSend(
            sink_, quantisation_buffer_.data(),
            samples_available * target_format_.real_bytes_per_sample(),
            portMAX_DELAY);
        assert(bytes_sent ==
               samples_available * target_format_.real_bytes_per_sample());
      }
    }
  }
}

template <>
auto SinkMixer::ConvertFixedToFloating<short>(InputStream& in_str,
                                              OutputStream& out_str) -> void {
  auto in = in_str.data_as<short>();
  auto out = out_str.data_as<float>();
  std::size_t samples_converted = std::min(in.size(), out.size());

  src_short_to_float_array(in.data(), out.data(), samples_converted);

  in_str.consume(samples_converted * sizeof(short));
  out_str.add(samples_converted * sizeof(float));
}

template <>
auto SinkMixer::ConvertFixedToFloating<int>(InputStream& in_str,
                                            OutputStream& out_str) -> void {
  auto in = in_str.data_as<int>();
  auto out = out_str.data_as<float>();
  std::size_t samples_converted = std::min(in.size(), out.size());

  src_int_to_float_array(in.data(), out.data(), samples_converted);

  in_str.consume(samples_converted * sizeof(int));
  out_str.add(samples_converted * sizeof(float));
}

auto SinkMixer::Resample(float src_ratio,
                         int channels,
                         InputStream& in,
                         OutputStream& out) -> void {
  if (resampler_ == nullptr || src_get_channels(resampler_) != channels) {
    if (resampler_ != nullptr) {
      src_delete(resampler_);
    }

    ESP_LOGI(kTag, "creating new resampler with %u channels", channels);

    int err = 0;
    resampler_ = src_new(SRC_LINEAR, channels, &err);
    assert(resampler_ != NULL);
    assert(err == 0);
  }

  auto in_buf = in.data_as<float>();
  auto out_buf = out.data_as<float>();

  src_set_ratio(resampler_, src_ratio);
  SRC_DATA args{
      .data_in = in_buf.data(),
      .data_out = out_buf.data(),
      .input_frames = static_cast<long>(in_buf.size()),
      .output_frames = static_cast<long>(out_buf.size()),
      .input_frames_used = 0,
      .output_frames_gen = 0,
      .end_of_input = 0,
      .src_ratio = src_ratio,
  };
  int err = src_process(resampler_, &args);
  if (err != 0) {
    ESP_LOGE(kTag, "resampler error: %s", src_strerror(err));
  }

  in.consume(args.input_frames_used * sizeof(float));
  out.add(args.output_frames_gen * sizeof(float));
}

template <>
auto SinkMixer::Quantise<short>(InputStream& in) -> std::size_t {
  auto src = in.data_as<float>();
  cpp::span<short> dest = quantisation_buffer_as_shorts_;
  dest = dest.first(std::min(src.size(), dest.size()));

  src_float_to_short_array(src.data(), dest.data(), dest.size());

  in.consume(dest.size() * sizeof(float));
  return dest.size();
}

template <>
auto SinkMixer::Quantise<int>(InputStream& in) -> std::size_t {
  auto src = in.data_as<float>();
  cpp::span<int> dest = quantisation_buffer_as_ints_;
  dest = dest.first(std::min<int>(src.size(), dest.size()));

  src_float_to_int_array(src.data(), dest.data(), dest.size());

  in.consume(dest.size() * sizeof(float));
  return dest.size();
}

}  // namespace audio
