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

#include "dac.hpp"
#include "gpio_expander.hpp"
#include "stream_info.hpp"

namespace audio {

class I2SAudioOutput : public IAudioSink {
 public:
  I2SAudioOutput(drivers::GpioExpander* expander,
                 std::weak_ptr<drivers::AudioDac> dac);
  ~I2SAudioOutput();

  auto Configure(const StreamInfo::Format& format) -> bool override;
  auto Send(const cpp::span<std::byte>& data) -> void override;
  auto Log() -> void override;

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 private:
  auto SetVolume(uint8_t volume) -> void;

  drivers::GpioExpander* expander_;
  std::shared_ptr<drivers::AudioDac> dac_;

  std::optional<StreamInfo::Pcm> current_config_;
};

}  // namespace audio
