/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "audio_element.hpp"
#include "audio_sink.hpp"
#include "chunk.hpp"
#include "result.hpp"

#include "gpios.hpp"
#include "i2s_dac.hpp"
#include "stream_info.hpp"

namespace audio {

class I2SAudioOutput : public IAudioSink {
 public:
  I2SAudioOutput(drivers::IGpios* expander, std::weak_ptr<drivers::I2SDac> dac);
  ~I2SAudioOutput();

  auto SetInUse(bool) -> void override;

  auto SetVolumeImbalance(int_fast8_t balance) -> void override;
  auto SetVolume(uint_fast8_t percent) -> void override;
  auto GetVolume() -> uint_fast8_t override;
  auto AdjustVolumeUp() -> bool override;
  auto AdjustVolumeDown() -> bool override;

  auto Configure(const StreamInfo::Format& format) -> bool override;
  auto Send(const cpp::span<std::byte>& data) -> void override;

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 private:
  auto GetAdjustedMaxAttenuation() -> int_fast8_t;

  drivers::IGpios* expander_;
  std::shared_ptr<drivers::I2SDac> dac_;

  std::optional<StreamInfo::Pcm> current_config_;
  int_fast8_t left_difference_;
  uint_fast8_t attenuation_;
};

}  // namespace audio
