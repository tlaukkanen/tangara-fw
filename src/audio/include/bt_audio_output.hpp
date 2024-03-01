/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "result.hpp"

#include "audio_sink.hpp"
#include "bluetooth.hpp"
#include "gpios.hpp"
#include "i2s_dac.hpp"
#include "tasks.hpp"

namespace audio {

class BluetoothAudioOutput : public IAudioOutput {
 public:
  BluetoothAudioOutput(StreamBufferHandle_t,
                       drivers::Bluetooth& bt,
                       tasks::WorkerPool&);
  ~BluetoothAudioOutput();

  auto SetMode(Modes) -> void override;

  auto SetVolumeImbalance(int_fast8_t balance) -> void override;

  auto SetVolume(uint16_t) -> void override;

  auto GetVolume() -> uint16_t override;

  auto GetVolumePct() -> uint_fast8_t override;
  auto SetVolumePct(uint_fast8_t val) -> bool override;
  auto GetVolumeDb() -> int_fast16_t override;
  auto SetVolumeDb(int_fast16_t) -> bool override;

  auto AdjustVolumeUp() -> bool override;
  auto AdjustVolumeDown() -> bool override;

  auto PrepareFormat(const Format&) -> Format override;
  auto Configure(const Format& format) -> void override;

  BluetoothAudioOutput(const BluetoothAudioOutput&) = delete;
  BluetoothAudioOutput& operator=(const BluetoothAudioOutput&) = delete;

 private:
  drivers::Bluetooth& bluetooth_;
  tasks::WorkerPool& bg_worker_;
  uint8_t volume_;
};

}  // namespace audio
