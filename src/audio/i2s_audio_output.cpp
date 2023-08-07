/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "i2s_audio_output.hpp"
#include <stdint.h>
#include <sys/_stdint.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "esp_err.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "freertos/projdefs.h"
#include "gpios.hpp"
#include "i2c.hpp"
#include "i2s_dac.hpp"
#include "result.hpp"
#include "stream_info.hpp"
#include "wm8523.hpp"

static const char* kTag = "I2SOUT";

namespace audio {

// Consumer line level = 0.316 VRMS = -10db = 61
// Professional line level = 1.228 VRMS = +4dB = 111
// Cliipping level = 2.44 VRMS = 133?
// all into 650 ohms

static constexpr uint16_t kMaxVolume = 0x1ff;
static constexpr uint16_t kMinVolume = 0b0;
static constexpr uint16_t kMaxVolumeBeforeClipping = 0x185;
static constexpr uint16_t kLineLevelVolume = 0x13d;
static constexpr uint16_t kDefaultVolume = 0x128;

I2SAudioOutput::I2SAudioOutput(drivers::IGpios* expander,
                               std::weak_ptr<drivers::I2SDac> dac)
    : expander_(expander),
      dac_(dac.lock()),
      current_config_(),
      left_difference_(0),
      current_volume_(kDefaultVolume),
      max_volume_(kLineLevelVolume) {
  SetVolume(GetVolume());
  dac_->SetSource(stream());
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
  left_difference_ = balance;
  SetVolume(GetVolume());
}

auto I2SAudioOutput::SetVolume(uint_fast8_t percent) -> void {
  percent = std::min<uint_fast8_t>(percent, 100);
  float new_value = static_cast<float>(max_volume_) / 100 * percent;
  current_volume_ = std::max<float>(new_value, kMinVolume);
  ESP_LOGI(kTag, "set volume to %u%% = %u", percent, current_volume_);

  int32_t left_unclamped = current_volume_ + left_difference_;
  uint16_t left = std::clamp<int32_t>(left_unclamped, kMinVolume, max_volume_);

  using drivers::wm8523::Register;
  drivers::wm8523::WriteRegister(Register::kDacGainLeft, left);
  drivers::wm8523::WriteRegister(Register::kDacGainRight,
                                 current_volume_ | 0x200);
}

auto I2SAudioOutput::GetVolume() -> uint_fast8_t {
  return static_cast<uint_fast8_t>(static_cast<float>(current_volume_) /
                                   max_volume_ * 100.0f);
}

auto I2SAudioOutput::AdjustVolumeUp() -> bool {
  if (GetVolume() >= 100) {
    return false;
  }
  if (GetVolume() >= 95) {
    SetVolume(100);
  } else {
    SetVolume(GetVolume() + 5);
  }
  return true;
}

auto I2SAudioOutput::AdjustVolumeDown() -> bool {
  if (GetVolume() == 0) {
    return false;
  }
  if (GetVolume() <= 5) {
    SetVolume(0);
  } else {
    SetVolume(GetVolume() - 5);
  }
  return true;
}

auto I2SAudioOutput::PrepareFormat(const StreamInfo::Pcm& orig)
    -> StreamInfo::Pcm {
  /*
return StreamInfo::Pcm{
    .channels = std::min<uint8_t>(orig.channels, 2),
    .bits_per_sample = std::clamp<uint8_t>(orig.bits_per_sample, 16, 32),
    .sample_rate = std::clamp<uint32_t>(orig.sample_rate, 8000, 96000),
};
  */
  return StreamInfo::Pcm{
      .channels = std::min<uint8_t>(orig.channels, 2),
      .bits_per_sample = 16,
      //.sample_rate = std::clamp<uint32_t>(orig.sample_rate, 8000, 96000),
      .sample_rate = 44100,
  };
}

auto I2SAudioOutput::Configure(const StreamInfo::Pcm& pcm) -> void {
  if (current_config_ && pcm == *current_config_) {
    ESP_LOGI(kTag, "ignoring unchanged format");
    return;
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
      return;
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
      return;
  }

  drivers::I2SDac::SampleRate sample_rate;
  switch (pcm.sample_rate) {
    case 8000:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_8;
      break;
    case 32000:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_32;
      break;
    case 44100:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_44_1;
      break;
    case 48000:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_48;
      break;
    case 88200:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_88_2;
      break;
    case 96000:
      sample_rate = drivers::I2SDac::SAMPLE_RATE_96;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown rate");
      return;
  }

  dac_->Reconfigure(ch, bps, sample_rate);
  current_config_ = pcm;
}

}  // namespace audio
