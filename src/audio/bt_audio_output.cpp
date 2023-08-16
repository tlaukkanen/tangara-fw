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

static const char* kTag = "BTOUT";

namespace audio {

static constexpr size_t kDrainBufferSize = 48 * 1024;

BluetoothAudioOutput::BluetoothAudioOutput(drivers::Bluetooth* bt)
    : IAudioSink(kDrainBufferSize, MALLOC_CAP_SPIRAM), bluetooth_(bt) {}

BluetoothAudioOutput::~BluetoothAudioOutput() {}

auto BluetoothAudioOutput::SetInUse(bool in_use) -> void {
  if (in_use) {
    bluetooth_->SetSource(stream());
  } else {
    bluetooth_->SetSource(nullptr);
  }
}

auto BluetoothAudioOutput::SetVolumeImbalance(int_fast8_t balance) -> void {}

auto BluetoothAudioOutput::SetVolume(uint_fast8_t percent) -> void {}

auto BluetoothAudioOutput::GetVolume() -> uint_fast8_t {
  return 50;
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
