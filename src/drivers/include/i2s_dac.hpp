/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/ringbuf.h"
#include "freertos/stream_buffer.h"
#include "result.hpp"
#include "span.hpp"

#include "gpios.hpp"
#include "sys/_stdint.h"

namespace drivers {

/**
 * Interface for a DAC that receives PCM samples over I2S.
 */
class I2SDac {
 public:
  static auto create(IGpios* expander) -> std::optional<I2SDac*>;

  I2SDac(IGpios* gpio, i2s_chan_handle_t i2s_handle);
  ~I2SDac();

  auto Start() -> void;
  auto Stop() -> void;

  enum Channels {
    CHANNELS_MONO,
    CHANNELS_STEREO,
  };
  enum BitsPerSample {
    BPS_16 = I2S_DATA_BIT_WIDTH_16BIT,
    BPS_24 = I2S_DATA_BIT_WIDTH_24BIT,
    BPS_32 = I2S_DATA_BIT_WIDTH_32BIT,
  };
  enum SampleRate {
    SAMPLE_RATE_11_025 = 11025,
    SAMPLE_RATE_16 = 16000,
    SAMPLE_RATE_22_05 = 22050,
    SAMPLE_RATE_32 = 32000,
    SAMPLE_RATE_44_1 = 44100,
    SAMPLE_RATE_48 = 48000,
    SAMPLE_RATE_96 = 96000,
    SAMPLE_RATE_192 = 192000,
  };

  auto Reconfigure(Channels ch, BitsPerSample bps, SampleRate rate) -> void;

  auto WriteData(const cpp::span<const std::byte>& data) -> void;
  auto SetSource(StreamBufferHandle_t buffer) -> void;

  // Not copyable or movable.
  I2SDac(const I2SDac&) = delete;
  I2SDac& operator=(const I2SDac&) = delete;

 private:
  IGpios* gpio_;
  i2s_chan_handle_t i2s_handle_;
  bool i2s_active_;
  StreamBufferHandle_t buffer_;

  i2s_std_clk_config_t clock_config_;
  i2s_std_slot_config_t slot_config_;
};

}  // namespace drivers
