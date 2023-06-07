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
  if (current_codec_ != nullptr && current_input_format_) {
    auto cur_encoding = std::get<StreamInfo::Encoded>(*current_input_format_);
    if (cur_encoding.type == encoded.type) {
      ESP_LOGI(kTag, "reusing existing decoder");
      current_input_format_ = info.format;
      return true;
    }
  }
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
  if (std::holds_alternative<std::monostate>(info.format) ||
      info.bytes_in_stream == 0) {
    // TODO(jacqueline): should we clear the stream format?
    // output->prepare({});
    return;
  }

  if (!current_input_format_ || *current_input_format_ != info.format) {
    // The input stream has changed! Immediately throw everything away and
    // start from scratch.
    has_samples_to_send_ = false;
    ProcessStreamInfo(info);
  }

  current_codec_->SetInput(input->data());

  while (true) {
    if (has_samples_to_send_) {
      auto format = current_codec_->GetOutputFormat();
      if (format.has_value()) {
        current_output_format_ = StreamInfo::Pcm{
            .channels = format->num_channels,
            .bits_per_sample = format->bits_per_sample,
            .sample_rate = format->sample_rate_hz,
        };

        if (!output->prepare(*current_output_format_)) {
          break;
        }

        auto write_res = current_codec_->WriteOutputSamples(output->data());
        output->add(write_res.first);
        has_samples_to_send_ = !write_res.second;

        if (has_samples_to_send_) {
          // We weren't able to fit all the generated samples into the output
          // buffer. Stop trying; we'll finish up during the next pass.
          break;
        }
      }
    }

    auto res = current_codec_->ProcessNextFrame();
    if (res.has_error()) {
      // TODO(jacqueline): Handle errors.
      return;
    }

    has_input_remaining_ = !res.value();
    if (!has_input_remaining_) {
      // We're out of useable data in this buffer. Finish immediately; there's
      // nothing to send.
      input->mark_incomplete();
      break;
    } else {
      has_samples_to_send_ = true;
    }
  }

  std::size_t pos = current_codec_->GetInputPosition();
  if (pos > 0) {
    input->consume(pos - 1);
  }
}

}  // namespace audio
