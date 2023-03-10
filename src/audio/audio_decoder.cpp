#include "audio_decoder.hpp"

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>

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
      stream_info_({}),
      has_samples_to_send_(false),
      needs_more_input_(true) {}

AudioDecoder::~AudioDecoder() {}

auto AudioDecoder::ProcessStreamInfo(const StreamInfo& info) -> bool {
  if (!std::holds_alternative<StreamInfo::Encoded>(info.data)) {
    return false;
  }
  const auto& encoded = std::get<StreamInfo::Encoded>(info.data);

  // Reuse the existing codec if we can. This will help with gapless playback,
  // since we can potentially just continue to decode as we were before,
  // without any setup overhead.
  // TODO: use audio type from stream
  if (current_codec_ != nullptr &&
      current_codec_->CanHandleType(encoded.type)) {
    current_codec_->ResetForNewStream();
    return true;
  }

  // TODO: use audio type from stream
  auto result = codecs::CreateCodecForType(encoded.type);
  if (result.has_value()) {
    current_codec_ = std::move(result.value());
  } else {
    ESP_LOGE(kTag, "no codec for this file");
    return false;
  }

  return true;
}

auto AudioDecoder::Process(std::vector<Stream>* inputs, MutableStream* output)
    -> void {
  // We don't really expect multiple inputs, so just pick the first that
  // contains data. If none of them contain data, then we can still flush
  // pending samples.
  auto input =
      std::find_if(inputs->begin(), inputs->end(),
                   [](const Stream& s) { return s.data.size_bytes() > 0; });

  if (input != inputs->end()) {
    const StreamInfo* info = input->info;
    if (!stream_info_ || *stream_info_ != *info) {
      // The input stream has changed! Immediately throw everything away and
      // start from scratch.
      // TODO: special case gapless playback? needs thought.
      stream_info_ = *info;
      has_samples_to_send_ = false;
      has_set_stream_info_ = false;

      ProcessStreamInfo(*info);
    }

    current_codec_->SetInput(input->data);
  }

  while (true) {
    if (has_samples_to_send_) {
      if (!has_set_stream_info_) {
        has_set_stream_info_ = true;
        auto format = current_codec_->GetOutputFormat();
        output->info->data.emplace<StreamInfo::Pcm>(
            format.bits_per_sample, format.sample_rate_hz, format.num_channels);
      }

      auto write_res = current_codec_->WriteOutputSamples(
          output->data.subspan(output->info->bytes_in_stream));
      output->info->bytes_in_stream += write_res.first;
      has_samples_to_send_ = !write_res.second;

      if (has_samples_to_send_) {
        // We weren't able to fit all the generated samples into the output
        // buffer. Stop trying; we'll finish up during the next pass.
        break;
      }
    }

    if (input != inputs->end()) {
      auto res = current_codec_->ProcessNextFrame();
      if (res.has_error()) {
        // TODO(jacqueline): Handle errors.
        return;
      }
      input->data = input->data.subspan(current_codec_->GetInputPosition());

      if (res.value()) {
        // We're out of data in this buffer. Finish immediately; there's nothing
        // to send.
        break;
      } else {
        has_samples_to_send_ = true;
      }
    } else {
      // No input; nothing to do.
      break;
    }
  }
}

}  // namespace audio
