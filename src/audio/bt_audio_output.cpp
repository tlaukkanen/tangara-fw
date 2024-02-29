/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bt_audio_output.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "gpios.hpp"
#include "i2c.hpp"
#include "i2s_dac.hpp"
#include "result.hpp"
#include "tasks.hpp"
#include "wm8523.hpp"

[[maybe_unused]] static const char* kTag = "BTOUT";

namespace audio {

BluetoothAudioOutput::BluetoothAudioOutput(StreamBufferHandle_t s,
                                           drivers::Bluetooth& bt,
                                           tasks::WorkerPool& p)
    : IAudioOutput(s), bluetooth_(bt), bg_worker_(p), volume_(10) {}

BluetoothAudioOutput::~BluetoothAudioOutput() {}

auto BluetoothAudioOutput::changeMode(Modes mode) -> void {
  if (mode == Modes::kOnPlaying) {
    bluetooth_.SetSource(stream());
  } else {
    bluetooth_.SetSource(nullptr);
  }
}

auto BluetoothAudioOutput::SetVolumeImbalance(int_fast8_t balance) -> void {}

auto BluetoothAudioOutput::SetVolume(uint16_t v) -> void {
  volume_ = std::clamp<uint16_t>(v, 0, 0x7f);
}

auto BluetoothAudioOutput::GetVolume() -> uint16_t {
  return volume_;
}

auto BluetoothAudioOutput::GetVolumePct() -> uint_fast8_t {
  return static_cast<uint_fast8_t>(static_cast<int>(volume_) * 100 / 0x7f);
}

auto BluetoothAudioOutput::GetVolumeDb() -> int_fast16_t {
  return 0;
}

auto BluetoothAudioOutput::AdjustVolumeUp() -> bool {
  if (volume_ == 0x7f) {
    return false;
  }
  volume_++;
  bg_worker_.Dispatch<void>([&]() { bluetooth_.SetVolume(volume_); });
  return true;
}

auto BluetoothAudioOutput::AdjustVolumeDown() -> bool {
  if (volume_ == 0) {
    return false;
  }
  volume_--;
  bg_worker_.Dispatch<void>([&]() { bluetooth_.SetVolume(volume_); });
  return true;
}

auto BluetoothAudioOutput::PrepareFormat(const Format& orig) -> Format {
  // ESP-IDF's current Bluetooth implementation currently handles SBC encoding,
  // but requires a fixed input format.
  return Format{
      .sample_rate = 44100,
      .num_channels = 2,
      .bits_per_sample = 16,
  };
}

auto BluetoothAudioOutput::Configure(const Format& fmt) -> void {
  // No configuration necessary; the output format is fixed.
}

}  // namespace audio
