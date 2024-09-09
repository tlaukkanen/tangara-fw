/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tts/player.hpp"

#include "codec.hpp"
#include "esp_log.h"
#include "sample.hpp"
#include "types.hpp"

namespace tts {

[[maybe_unused]] static constexpr char kTag[] = "ttsplay";

Player::Player(tasks::WorkerPool& worker,
               drivers::PcmBuffer& output,
               audio::FatfsStreamFactory& factory)
    : bg_(worker), stream_factory_(factory), output_(output) {}

auto Player::playFile(const std::string& path) -> void {
  ESP_LOGI(kTag, "playing '%s'", path.c_str());
  bg_.Dispatch<void>([=]() {
    auto stream = stream_factory_.create(path);
    if (!stream) {
      ESP_LOGE(kTag, "creating stream failed");
      return;
    }
    if (stream->type() != codecs::StreamType::kWav) {
      ESP_LOGE(kTag, "stream was unsupported type");
      return;
    }
    auto decoder = codecs::CreateCodecForType(stream->type());
    if (!decoder) {
      ESP_LOGE(kTag, "creating decoder failed");
      return;
    }
    std::unique_ptr<codecs::ICodec> codec{*decoder};
    auto open_res = codec->OpenStream(stream, 0);
    if (open_res.has_error()) {
      ESP_LOGE(kTag, "opening stream failed");
      return;
    }
    // if (open_res->sample_rate_hz != 48000 || open_res->num_channels != 2) {
    // ESP_LOGE(kTag, "stream format is wrong (was %u channels @ %lu hz)",
    // open_res->num_channels, open_res->sample_rate_hz);
    // return;
    // }
    sample::Sample decode_buf[4096];
    for (;;) {
      auto decode_res = codec->DecodeTo(decode_buf);
      if (decode_res.has_error()) {
        ESP_LOGE(kTag, "decoding error");
        return;
      }
      if (decode_res->is_stream_finished) {
        break;
      }

      std::span<sample::Sample> decode_span{decode_buf,
                                            decode_res->samples_written};
      while (!decode_span.empty()) {
        size_t sent = output_.send(decode_span);
        decode_span = decode_span.subspan(sent);
      }
    }

    ESP_LOGI(kTag, "finished playing okay");
  });
}

}  // namespace tts
