/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "i2s_audio_output.hpp"
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <variant>

#include "esp_err.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "freertos/projdefs.h"
#include "gpios.hpp"
#include "i2s_dac.hpp"
#include "result.hpp"
#include "stream_info.hpp"

static const char* kTag = "I2SOUT";

namespace audio {

I2SAudioOutput::I2SAudioOutput(drivers::IGpios* expander,
                               std::weak_ptr<drivers::I2SDac> dac)
    : expander_(expander),
      dac_(dac.lock()),
      current_config_(),
      left_difference_(0),
      attenuation_() {
  SetVolume(25);  // For testing
  dac_->SetSource(buffer());
}

I2SAudioOutput::~I2SAudioOutput() {
  dac_->Stop();
  dac_->SetSource(nullptr);
}

auto I2SAudioOutput::SetInUse(bool in_use) -> void {
  if (in_use) {
    dac_->Start();
  } else {
    dac_->Stop();
  }
}

auto I2SAudioOutput::SetVolumeImbalance(int_fast8_t balance) -> void {
  // TODO.
}

auto I2SAudioOutput::SetVolume(uint_fast8_t percent) -> void {
  // TODO.
}

auto I2SAudioOutput::GetVolume() -> uint_fast8_t {
  // TODO.
  return 100;
}

auto I2SAudioOutput::GetAdjustedMaxAttenuation() -> int_fast8_t {
  // TODO
  return 0;
}

auto I2SAudioOutput::AdjustVolumeUp() -> bool {
  // TODO
  return true;
}

auto I2SAudioOutput::AdjustVolumeDown() -> bool {
  // TODO
  return true;
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

  ESP_LOGI(kTag, "incoming audio stream: %u ch %u bpp @ %lu Hz", pcm.channels,
           pcm.bits_per_sample, pcm.sample_rate);

  drivers::I2SDac::Channels ch;
  switch (pcm.channels) {
    case 1:
      ch = drivers::I2SDac::CHANNELS_MONO;
      break;
    case 2:
      ch = drivers::I2SDac::CHANNELS_STEREO;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with out of bounds channels");
      return false;
  }

  drivers::I2SDac::BitsPerSample bps;
  switch (pcm.bits_per_sample) {
    case 16:
      bps = drivers::I2SDac::BPS_16;
      break;
    case 24:
      bps = drivers::I2SDac::BPS_24;
      break;
    case 32:
      bps = drivers::I2SDac::BPS_32;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown bps");
      return false;
  }

  drivers::I2SDac::SampleRate sample_rate;
  switch (pcm.sample_rate) {
    case 44100:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_44_1;
      break;
    case 48000:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_48;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown rate");
      return false;
  }

  dac_->Reconfigure(ch, bps, sample_rate);
  current_config_ = pcm;

  return true;
}

auto I2SAudioOutput::Send(const cpp::span<std::byte>& data) -> void {
  dac_->WriteData(data);
}

}  // namespace audio
