/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/bt_audio_output.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "drivers/bluetooth.hpp"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "drivers/gpios.hpp"
#include "drivers/i2c.hpp"
#include "drivers/i2s_dac.hpp"
#include "drivers/pcm_buffer.hpp"
#include "drivers/wm8523.hpp"
#include "result.hpp"
#include "tasks.hpp"

[[maybe_unused]] static const char* kTag = "BTOUT";

namespace audio {

static constexpr uint16_t kVolumeRange = 60;

using ConnectionState = drivers::Bluetooth::ConnectionState;

BluetoothAudioOutput::BluetoothAudioOutput(drivers::Bluetooth& bt,
                                           drivers::PcmBuffer& buffer,
                                           tasks::WorkerPool& p)
    : IAudioOutput(),
      bluetooth_(bt),
      buffer_(buffer),
      bg_worker_(p),
      volume_() {}

BluetoothAudioOutput::~BluetoothAudioOutput() {}

auto BluetoothAudioOutput::changeMode(Modes mode) -> void {
  if (mode == Modes::kOnPlaying) {
    bluetooth_.source(&buffer_);
  } else {
    bluetooth_.source(nullptr);
  }
}

auto BluetoothAudioOutput::SetVolumeImbalance(int_fast8_t balance) -> void {
  // FIXME: Support two separate scaling factors in the bluetooth driver.
}

auto BluetoothAudioOutput::SetVolume(uint16_t v) -> void {
  volume_ = std::clamp<uint16_t>(v, 0, 100);
  bg_worker_.Dispatch<void>([&]() {
    float factor =
        pow(10, static_cast<double>(kVolumeRange) * (volume_ - 100) / 100 / 20);
    bluetooth_.softVolume(factor);
  });
}

auto BluetoothAudioOutput::GetVolume() -> uint16_t {
  return volume_;
}

auto BluetoothAudioOutput::GetVolumePct() -> uint_fast8_t {
  return static_cast<uint_fast8_t>(round(static_cast<int>(volume_)));
}

auto BluetoothAudioOutput::SetVolumePct(uint_fast8_t val) -> bool {
  if (val > 100) {
    return false;
  }
  SetVolume(val);
  return true;
}

auto BluetoothAudioOutput::GetVolumeDb() -> int_fast16_t {
  double pct = GetVolumePct() / 100.0;
  if (pct <= 0) {
    pct = 0.01;
  }
  int_fast16_t db = log(pct) * 20;
  return db;
}

auto BluetoothAudioOutput::SetVolumeDb(int_fast16_t val) -> bool {
  double pct = exp(val / 20.0) * 100;
  return SetVolumePct(pct);
}

auto BluetoothAudioOutput::AdjustVolumeUp() -> bool {
  if (volume_ == 100) {
    return false;
  }
  volume_++;
  SetVolume(volume_);
  return true;
}

auto BluetoothAudioOutput::AdjustVolumeDown() -> bool {
  if (volume_ == 0) {
    return false;
  }
  volume_--;
  SetVolume(volume_);
  return true;
}

auto BluetoothAudioOutput::PrepareFormat(const Format& orig) -> Format {
  // ESP-IDF's current Bluetooth implementation currently handles SBC encoding,
  // but requires a fixed input format.
  return Format{
      .sample_rate = 48000,
      .num_channels = 2,
      .bits_per_sample = 16,
  };
}

auto BluetoothAudioOutput::Configure(const Format& fmt) -> void {
  // No configuration necessary; the output format is fixed.
}

}  // namespace audio
