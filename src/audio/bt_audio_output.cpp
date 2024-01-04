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
#include "wm8523.hpp"

[[maybe_unused]] static const char* kTag = "BTOUT";

namespace audio {

BluetoothAudioOutput::BluetoothAudioOutput(StreamBufferHandle_t s,
                                           drivers::Bluetooth& bt)
    : IAudioOutput(s), bluetooth_(bt) {}

BluetoothAudioOutput::~BluetoothAudioOutput() {}

auto BluetoothAudioOutput::SetMode(Modes mode) -> void {
  if (mode == Modes::kOnPlaying) {
    bluetooth_.SetSource(stream());
  } else {
    bluetooth_.SetSource(nullptr);
  }
}

auto BluetoothAudioOutput::SetVolumeImbalance(int_fast8_t balance) -> void {}

auto BluetoothAudioOutput::SetVolume(uint16_t) -> void {}

auto BluetoothAudioOutput::GetVolume() -> uint16_t {
  return 0;
}

auto BluetoothAudioOutput::GetVolumePct() -> uint_fast8_t {
  return 0;
}

auto BluetoothAudioOutput::GetVolumeDb() -> int_fast16_t {
  return 0;
}

auto BluetoothAudioOutput::AdjustVolumeUp() -> bool {
  return false;
}

auto BluetoothAudioOutput::AdjustVolumeDown() -> bool {
  return false;
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
