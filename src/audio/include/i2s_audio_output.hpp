/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
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

  auto PrepareFormat(const StreamInfo::Pcm&) -> StreamInfo::Pcm override;
  auto Configure(const StreamInfo::Pcm& format) -> void override;

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 private:
  drivers::IGpios* expander_;
  std::shared_ptr<drivers::I2SDac> dac_;

  std::optional<StreamInfo::Pcm> current_config_;
  int_fast8_t left_difference_;
  uint16_t current_volume_;
  uint16_t max_volume_;
};

}  // namespace audio
