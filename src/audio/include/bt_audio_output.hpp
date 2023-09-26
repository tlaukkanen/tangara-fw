/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "result.hpp"

#include "audio_sink.hpp"
#include "bluetooth.hpp"
#include "gpios.hpp"
#include "i2s_dac.hpp"

namespace audio {

class BluetoothAudioOutput : public IAudioOutput {
 public:
  BluetoothAudioOutput(StreamBufferHandle_t, drivers::Bluetooth& bt);
  ~BluetoothAudioOutput();

  auto SetInUse(bool) -> void override;

  auto SetVolumeImbalance(int_fast8_t balance) -> void override;
  auto SetVolume(uint_fast8_t percent) -> void override;
  auto GetVolume() -> uint_fast8_t override;
  auto AdjustVolumeUp() -> bool override;
  auto AdjustVolumeDown() -> bool override;

  auto PrepareFormat(const Format&) -> Format override;
  auto Configure(const Format& format) -> void override;

  BluetoothAudioOutput(const BluetoothAudioOutput&) = delete;
  BluetoothAudioOutput& operator=(const BluetoothAudioOutput&) = delete;

 private:
  drivers::Bluetooth& bluetooth_;
};

}  // namespace audio
