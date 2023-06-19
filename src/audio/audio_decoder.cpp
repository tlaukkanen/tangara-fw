/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_decoder.hpp"

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "codec.hpp"
#include "freertos/FreeRTOS.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "fatfs_audio_input.hpp"
#include "stream_info.hpp"

namespace audio {

static const char* kTag = "DEC";

AudioDecoder::AudioDecoder()
    : IAudioElement(),
      current_codec_(),
      current_input_format_(),
      current_output_format_(),
      has_prepared_output_(false),
      has_samples_to_send_(false),
      has_input_remaining_(false) {}

AudioDecoder::~AudioDecoder() {}

auto AudioDecoder::ProcessStreamInfo(const StreamInfo& info) -> bool {
  has_prepared_output_ = false;
  current_codec_.reset();
  current_input_format_.reset();
  current_output_format_.reset();

  if (!std::holds_alternative<StreamInfo::Encoded>(info.format)) {
    return false;
  }

  const auto& new_format = std::get<StreamInfo::Encoded>(info.format);
  current_input_format_ = info.format;

  ESP_LOGI(kTag, "creating new decoder");
  auto result = codecs::CreateCodecForType(new_format.type);
  if (result.has_value()) {
    current_codec_.reset(result.value());
  } else {
    ESP_LOGE(kTag, "no codec for this file");
    return false;
  }

  return true;
}

auto AudioDecoder::NeedsToProcess() const -> bool {
  return has_samples_to_send_ || has_input_remaining_;
}

auto AudioDecoder::Process(const std::vector<InputStream>& inputs,
                           OutputStream* output) -> void {
  auto input = inputs.begin();
  const StreamInfo& info = input->info();

  // Is this a completely new stream?
  if (!current_input_format_) {
    if (!ProcessStreamInfo(info)) {
      // We couldn't handle the new stream. Signal to the producer that we don't
      // have anything to do.
      input->mark_consumer_finished();
      return;
    }
  }

  // Have we determined what kind of samples this stream decodes to?
  if (!current_output_format_) {
    auto res = current_codec_->BeginStream(input->data());
    input->consume(res.first);

    if (res.second.has_error()) {
      auto err = res.second.error();
      if (err == codecs::ICodec::Error::kOutOfInput) {
        // We didn't manage to clear whatever front matter is before this
        // stream's header. We need to call BeginStream again with more data.
        return;
      }
      // Somthing about the stream's header was malformed. Skip it.
      ESP_LOGE(kTag, "error beginning stream");
      input->mark_consumer_finished();
      return;
    }

    // The stream started successfully. Record what format the samples are in.
    codecs::ICodec::OutputFormat format = res.second.value();
    current_output_format_ = StreamInfo::Pcm{
        .channels = format.num_channels,
        .bits_per_sample = format.bits_per_sample,
        .sample_rate = format.sample_rate_hz,
    };

    if (info.seek_to_seconds) {
      seek_to_sample_ = *info.seek_to_seconds * format.sample_rate_hz;
    } else {
      seek_to_sample_.reset();
    }
  }

  if (seek_to_sample_) {
    ESP_LOGI(kTag, "seeking forwards...");
    auto res = current_codec_->SeekStream(input->data(), *seek_to_sample_);
    input->consume(res.first);

    if (res.second.has_error()) {
      auto err = res.second.error();
      if (err == codecs::ICodec::Error::kOutOfInput) {
        return;
      } else {
        // TODO(jacqueline): Handle errors.
      }
    }

    seek_to_sample_.reset();
  }

  has_input_remaining_ = true;
  while (true) {
    // Make sure the output buffer is ready to receive samples in our format
    // before starting to process data.
    // TODO(jacqueline): Pass through seek info here?
    if (!has_prepared_output_ && !output->prepare(*current_output_format_)) {
      ESP_LOGI(kTag, "waiting for buffer to become free");
      return;
    }
    has_prepared_output_ = true;

    // Parse frames and produce samples.
    auto res = current_codec_->ContinueStream(input->data(), output->data());
    input->consume(res.first);

    // Handle any errors during processing.
    if (res.second.has_error()) {
      // The codec ran out of input during processing. This is expected to
      // happen throughout the stream.
      if (res.second.error() == codecs::ICodec::Error::kOutOfInput) {
        ESP_LOGI(kTag, "codec needs more data");
        has_input_remaining_ = false;
        has_samples_to_send_ = false;
        if (input->is_producer_finished()) {
          ESP_LOGI(kTag, "codec is all done.");

          // We're out of data, and so is the producer. Nothing left to be done
          // with the input stream.
          input->mark_consumer_finished();

          // Upstream isn't going to give us any more data. Tell downstream
          // that they shouldn't expact any more samples from this stream.
          output->mark_producer_finished();
          break;
        }
      } else {
        // TODO(jacqueline): Handle errors.
        ESP_LOGE(kTag, "codec returned fatal error");
      }
      // Note that a codec that returns an error is not allowed to write
      // samples. So it's safe to skip the latter part of the loop.
      return;
    } else {
      // Some samples were written! Ensure the downstream element knows about
      // them.
      codecs::ICodec::OutputInfo out_info = res.second.value();
      output->add(out_info.bytes_written);
      has_samples_to_send_ = !out_info.is_finished_writing;

      if (has_samples_to_send_) {
        // The codec wasn't able to finish writing all of its samples into the
        // output buffer. We need to return so that we can get a new buffer.
        return;
      }
    }
  }

  current_codec_.reset();
  current_input_format_.reset();
}

}  // namespace audio
