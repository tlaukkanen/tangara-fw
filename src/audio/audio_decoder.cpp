#include "audio_decoder.hpp"

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "cbor/tinycbor/src/cborinternal_p.h"
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
      has_samples_to_send_(false) {}

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
  if (current_codec_ != nullptr &&
      current_codec_->CanHandleType(encoded.type)) {
    current_codec_->ResetForNewStream();
    ESP_LOGI(kTag, "reusing existing decoder");
    return true;
  }

  // TODO: use audio type from stream
  auto result = codecs::CreateCodecForType(encoded.type);
  if (result.has_value()) {
    ESP_LOGI(kTag, "creating new decoder");
    current_codec_ = std::move(result.value());
  } else {
    ESP_LOGE(kTag, "no codec for this file");
    return false;
  }

  return true;
}

auto AudioDecoder::Process(const std::vector<InputStream>& inputs,
                           OutputStream* output) -> void {
  // We don't really expect multiple inputs, so just pick the first that
  // contains data. If none of them contain data, then we can still flush
  // pending samples.
  auto input = std::find_if(
      inputs.begin(), inputs.end(),
      [](const InputStream& s) { return s.data().size_bytes() > 0; });
  if (input == inputs.end()) {
    input = inputs.begin();
  }

  const StreamInfo& info = input->info();
  if (std::holds_alternative<std::monostate>(info.format)) {
    return;
  }
  if (!current_input_format_ || *current_input_format_ != info.format) {
    // The input stream has changed! Immediately throw everything away and
    // start from scratch.
    current_input_format_ = info.format;
    has_samples_to_send_ = false;

    ProcessStreamInfo(info);
  }

  current_codec_->SetInput(input->data());

  while (true) {
    if (has_samples_to_send_) {
      if (!current_output_format_) {
        auto format = current_codec_->GetOutputFormat();
        current_output_format_ = StreamInfo::Pcm{
            .channels = format.num_channels,
            .bits_per_sample = format.bits_per_sample,
            .sample_rate = format.sample_rate_hz,
        };
      }

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

    auto res = current_codec_->ProcessNextFrame();
    if (res.has_error()) {
      // TODO(jacqueline): Handle errors.
      return;
    }

    if (res.value()) {
      // We're out of useable data in this buffer. Finish immediately; there's
      // nothing to send.
      input->mark_incomplete();
      break;
    } else {
      has_samples_to_send_ = true;
    }
  }

  input->consume(current_codec_->GetInputPosition());
}

}  // namespace audio
