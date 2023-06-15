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
      has_samples_to_send_(false),
      has_input_remaining_(false) {}

AudioDecoder::~AudioDecoder() {}

auto AudioDecoder::ProcessStreamInfo(const StreamInfo& info) -> bool {
  if (!std::holds_alternative<StreamInfo::Encoded>(info.format)) {
    return false;
  }
  ESP_LOGI(kTag, "got new stream");
  const auto& encoded = std::get<StreamInfo::Encoded>(info.format);

  // Reuse the existing codec if we can. This will help with gapless playback,
  // since we can potentially just continue to decode as we were before,
  // without any setup overhead.
  // TODO(jacqueline): Reconsider this. It makes a lot of things harder to smash
  // streams together at this layer.
  /*
  if (current_codec_ != nullptr && current_input_format_) {
    auto cur_encoding = std::get<StreamInfo::Encoded>(*current_input_format_);
    if (cur_encoding.type == encoded.type) {
      ESP_LOGI(kTag, "reusing existing decoder");
      current_input_format_ = info.format;
      return true;
    }
  }
  */
  current_input_format_ = info.format;

  ESP_LOGI(kTag, "creating new decoder");
  auto result = codecs::CreateCodecForType(encoded.type);
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

  // Check the input stream's format has changed (or, by extension, if this is
  // the first stream).
  if (!current_input_format_ || *current_input_format_ != info.format) {
    ESP_LOGI(kTag, "beginning new stream");
    has_samples_to_send_ = false;
    ProcessStreamInfo(info);
    auto res = current_codec_->BeginStream(input->data());
    input->consume(res.first);
    if (res.second.has_error()) {
      // TODO(jacqueline): Handle errors.
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

  while (seek_to_sample_) {
    ESP_LOGI(kTag, "seeking forwards...");
    auto res = current_codec_->SeekStream(input->data(), *seek_to_sample_);
    input->consume(res.first);
    if (res.second.has_error()) {
      auto err = res.second.error();
      if (err == codecs::ICodec::Error::kOutOfInput) {
        return;
      } else {
        // TODO(jacqueline): Handle errors.
        seek_to_sample_.reset();
      }
    } else {
      seek_to_sample_.reset();
    }
  }

  has_input_remaining_ = true;
  while (true) {
    // TODO(jacqueline): Pass through seek info here?
    if (!output->prepare(*current_output_format_)) {
      ESP_LOGI(kTag, "waiting for buffer to become free");
      break;
    }

    auto res = current_codec_->ContinueStream(input->data(), output->data());
    input->consume(res.first);
    if (res.second.has_error()) {
      if (res.second.error() == codecs::ICodec::Error::kOutOfInput) {
        ESP_LOGW(kTag, "out of input");
        ESP_LOGW(kTag, "(%u bytes left)", input->data().size_bytes());
        has_input_remaining_ = false;
        // We can't be halfway through sending samples if the codec is asking
        // for more input.
        has_samples_to_send_ = false;
        input->mark_incomplete();
      } else {
        // TODO(jacqueline): Handle errors.
        ESP_LOGE(kTag, "codec return fatal error");
      }
      return;
    }

    codecs::ICodec::OutputInfo out_info = res.second.value();
    output->add(out_info.bytes_written);
    has_samples_to_send_ = !out_info.is_finished_writing;

    if (has_samples_to_send_) {
      // We weren't able to fit all the generated samples into the output
      // buffer. Stop trying; we'll finish up during the next pass.
      break;
    }
  }
}

}  // namespace audio
