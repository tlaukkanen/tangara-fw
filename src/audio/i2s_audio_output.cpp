/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "i2s_audio_output.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <variant>

#include "esp_err.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "dac.hpp"
#include "freertos/projdefs.h"
#include "gpio_expander.hpp"
#include "result.hpp"
#include "stream_info.hpp"

static const char* kTag = "I2SOUT";

namespace audio {

I2SAudioOutput::I2SAudioOutput(drivers::GpioExpander* expander,
                               std::weak_ptr<drivers::AudioDac> dac)
    : expander_(expander), dac_(dac.lock()), current_config_() {
  dac_->WriteVolume(127);  // for testing
  dac_->SetSource(buffer());
}

I2SAudioOutput::~I2SAudioOutput() {
  dac_->SetSource(nullptr);
}

auto I2SAudioOutput::Configure(const StreamInfo::Format& format) -> bool {
  if (!std::holds_alternative<StreamInfo::Pcm>(format)) {
    ESP_LOGI(kTag, "ignoring non-pcm stream (%d)", format.index());
    return false;
  }

  StreamInfo::Pcm pcm = std::get<StreamInfo::Pcm>(format);

  if (current_config_ && pcm == *current_config_) {
    ESP_LOGI(kTag, "ignoring unchanged format");
    return true;
  }

  ESP_LOGI(kTag, "incoming audio stream: %u bpp @ %lu Hz", pcm.bits_per_sample,
           pcm.sample_rate);

  drivers::AudioDac::BitsPerSample bps;
  switch (pcm.bits_per_sample) {
    case 16:
      bps = drivers::AudioDac::BPS_16;
      break;
    case 24:
      bps = drivers::AudioDac::BPS_24;
      break;
    case 32:
      bps = drivers::AudioDac::BPS_32;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown bps");
      return false;
  }

  drivers::AudioDac::SampleRate sample_rate;
  switch (pcm.sample_rate) {
    case 44100:
      sample_rate = drivers::AudioDac::SAMPLE_RATE_44_1;
      break;
    case 48000:
      sample_rate = drivers::AudioDac::SAMPLE_RATE_48;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown rate");
      return false;
  }

  // TODO(jacqueline): probs do something with the channel hey

  dac_->Reconfigure(bps, sample_rate);
  current_config_ = pcm;

  return true;
}

auto I2SAudioOutput::Send(const cpp::span<std::byte>& data) -> void {
  dac_->WriteData(data);
}

auto I2SAudioOutput::Log() -> void {
  dac_->LogStatus();
}

auto I2SAudioOutput::SetVolume(uint8_t volume) -> void {
  dac_->WriteVolume(volume);
}

}  // namespace audio
