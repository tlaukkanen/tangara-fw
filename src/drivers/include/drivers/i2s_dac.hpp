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
#include <span>
#include <utility>

#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "drivers/pcm_buffer.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/stream_buffer.h"
#include "result.hpp"

#include "drivers/gpios.hpp"
#include "sys/_stdint.h"

namespace drivers {

// DMA max buffer size for I2S is 4092. We normalise to 2-channel, 16 bit
// audio, which gives us a max of 4092 / 2 / 2 (16 bits) frames. This in turn
// means that at 48kHz, we have about 21ms of budget to fill each buffer.
// We base this off of the maximum DMA size in order to minimise the amount of
// work the CPU has to do to service the DMA callbacks.
constexpr size_t kI2SBufferLengthFrames = 1024;

/**
 * Interface for a DAC that receives PCM samples over I2S.
 */
class I2SDac {
 public:
  static auto create(IGpios& expander, OutputBuffers&)
      -> std::optional<I2SDac*>;

  I2SDac(IGpios& gpio, OutputBuffers&, i2s_chan_handle_t i2s_handle);
  ~I2SDac();

  auto SetPaused(bool) -> void;

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
    SAMPLE_RATE_8 = 8000,
    SAMPLE_RATE_32 = 32000,
    SAMPLE_RATE_44_1 = 44100,
    SAMPLE_RATE_48 = 48000,
    SAMPLE_RATE_88_2 = 88200,
    SAMPLE_RATE_96 = 96000,
  };

  auto Reconfigure(Channels ch, BitsPerSample bps, SampleRate rate) -> void;

  auto WriteData(const std::span<const std::byte>& data) -> void;

  // Not copyable or movable.
  I2SDac(const I2SDac&) = delete;
  I2SDac& operator=(const I2SDac&) = delete;

 private:
  auto set_channel(bool) -> void;

  IGpios& gpio_;
  OutputBuffers& buffers_;
  i2s_chan_handle_t i2s_handle_;

  bool i2s_active_;
  std::mutex configure_mutex_;

  i2s_std_clk_config_t clock_config_;
  i2s_std_slot_config_t slot_config_;
};

}  // namespace drivers
