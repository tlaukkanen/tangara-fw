/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "i2s_audio_output.hpp"
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "audio_sink.hpp"
#include "gpios.hpp"
#include "i2c.hpp"
#include "i2s_dac.hpp"
#include "result.hpp"
#include "wm8523.hpp"

[[maybe_unused]] static const char* kTag = "I2SOUT";

namespace audio {

// Consumer line level = 0.316 VRMS = -10db = 61
// Professional line level = 1.228 VRMS = +4dB = 111
// Cliipping level = 2.44 VRMS = 133?
// all into 650 ohms

static constexpr uint16_t kMaxVolume = 0x1ff;
static constexpr uint16_t kMinVolume = 0b0;
static constexpr uint16_t kMaxVolumeBeforeClipping = 0x185;
static constexpr uint16_t kLineLevelVolume = 0x13d;
static constexpr uint16_t kDefaultVolume = 0x100;

static constexpr size_t kDrainBufferSize = 8 * 1024;

I2SAudioOutput::I2SAudioOutput(StreamBufferHandle_t s,
                               drivers::IGpios& expander)
    : IAudioOutput(s),
      expander_(expander),
      dac_(),
      current_mode_(Modes::kOff),
      current_config_(),
      left_difference_(0),
      current_volume_(kDefaultVolume),
      max_volume_(0) {}

I2SAudioOutput::~I2SAudioOutput() {
  dac_->Stop();
  dac_->SetSource(nullptr);
}

auto I2SAudioOutput::changeMode(Modes mode) -> void {
  if (mode == current_mode_) {
    return;
  }
  if (mode == Modes::kOff) {
    dac_->Stop();
    dac_.reset();
    return;
  } else if (current_mode_ == Modes::kOff) {
    auto instance = drivers::I2SDac::create(expander_);
    if (!instance) {
      return;
    }
    SetVolume(GetVolume());
    dac_.reset(*instance);
    dac_->SetSource(stream());
    dac_->Start();
  }
  current_mode_ = mode;
  dac_->SetPaused(mode == Modes::kOnPaused);
}

auto I2SAudioOutput::SetVolumeImbalance(int_fast8_t balance) -> void {
  left_difference_ = balance;
  SetVolume(GetVolume());
}

auto I2SAudioOutput::SetMaxVolume(uint16_t max) -> void {
  max_volume_ = std::clamp(max, drivers::wm8523::kAbsoluteMinVolume,
                           drivers::wm8523::kAbsoluteMaxVolume);
  SetVolume(GetVolume());
}

auto I2SAudioOutput::SetVolume(uint16_t vol) -> void {
  current_volume_ = std::clamp(vol, kMinVolume, max_volume_);

  int32_t left_unclamped = current_volume_ + left_difference_;
  uint16_t left = std::clamp<int32_t>(left_unclamped, kMinVolume, max_volume_);

  using drivers::wm8523::Register;
  drivers::wm8523::WriteRegister(Register::kDacGainLeft, left);
  drivers::wm8523::WriteRegister(Register::kDacGainRight,
                                 current_volume_ | 0x200);
}

auto I2SAudioOutput::GetVolume() -> uint16_t {
  return current_volume_;
}

auto I2SAudioOutput::GetVolumePct() -> uint_fast8_t {
  return (current_volume_ - kMinVolume) * 100 / (max_volume_ - kMinVolume);
}

auto I2SAudioOutput::SetVolumePct(uint_fast8_t val) -> bool {
  if (val > 100) {
    return false;
  }
  uint16_t vol = (val * (max_volume_ - kMinVolume))/100 + kMinVolume;
  SetVolume(vol);
  return true;
}

auto I2SAudioOutput::GetVolumeDb() -> int_fast16_t {
  // Add two before dividing in order to round correctly.
  return (static_cast<int>(current_volume_) -
          static_cast<int>(drivers::wm8523::kLineLevelReferenceVolume) + 2) /
         4;
}

auto I2SAudioOutput::SetVolumeDb(int_fast16_t val) -> bool {
  SetVolume(val * 4 + static_cast<int>(drivers::wm8523::kLineLevelReferenceVolume) - 2);
  return true;
}

auto I2SAudioOutput::AdjustVolumeUp() -> bool {
  if (GetVolume() >= max_volume_) {
    return false;
  }
  SetVolume(GetVolume() + 1);
  return true;
}

auto I2SAudioOutput::AdjustVolumeDown() -> bool {
  if (GetVolume() == kMinVolume) {
    return false;
  }
  if (GetVolume() <= kMinVolume + 1) {
    SetVolume(0);
  } else {
    SetVolume(GetVolume() - 1);
  }
  return true;
}

auto I2SAudioOutput::PrepareFormat(const Format& orig) -> Format {
  return Format{
      .sample_rate = std::clamp<uint32_t>(orig.sample_rate, 8000, 96000),
      .num_channels = std::min<uint8_t>(orig.num_channels, 2),
      .bits_per_sample = std::clamp<uint8_t>(orig.bits_per_sample, 16, 32),
  };
}

auto I2SAudioOutput::Configure(const Format& fmt) -> void {
  if (current_config_ && fmt == *current_config_) {
    ESP_LOGI(kTag, "ignoring unchanged format");
    return;
  }

  drivers::I2SDac::Channels ch;
  switch (fmt.num_channels) {
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
  switch (fmt.bits_per_sample) {
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
  switch (fmt.sample_rate) {
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
  current_config_ = fmt;
}

}  // namespace audio
