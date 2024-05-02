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

#include "audio/audio_sink.hpp"
#include "drivers/gpios.hpp"
#include "drivers/i2s_dac.hpp"
#include "result.hpp"

namespace audio {

class I2SAudioOutput : public IAudioOutput {
 public:
  I2SAudioOutput(StreamBufferHandle_t, drivers::IGpios& expander);
  ~I2SAudioOutput();

  auto SetMaxVolume(uint16_t) -> void;
  auto SetVolumeDb(uint16_t) -> void;

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

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 protected:
  auto changeMode(Modes) -> void override;

 private:
  drivers::IGpios& expander_;
  std::unique_ptr<drivers::I2SDac> dac_;

  Modes current_mode_;
  std::optional<Format> current_config_;
  int_fast8_t left_difference_;
  uint16_t current_volume_;
  uint16_t max_volume_;
};

}  // namespace audio
