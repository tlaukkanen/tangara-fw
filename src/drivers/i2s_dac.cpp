/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/i2s_dac.hpp"
#include <stdint.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

#include "assert.h"
#include "driver/i2c.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "drivers/pcm_buffer.hpp"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/ringbuf.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "drivers/gpios.hpp"
#include "drivers/i2c.hpp"
#include "drivers/wm8523.hpp"
#include "hal/i2s_types.h"
#include "soc/clk_tree_defs.h"

namespace drivers {

[[maybe_unused]] static const char* kTag = "i2s_dac";
static const i2s_port_t kI2SPort = I2S_NUM_0;

DRAM_ATTR static volatile bool sSwapWords = false;

extern "C" IRAM_ATTR auto callback(i2s_chan_handle_t handle,
                                   i2s_event_data_t* event,
                                   void* user_ctx) -> bool {
  if (event == nullptr || user_ctx == nullptr) {
    return false;
  }
  if (event->dma_buf == nullptr || event->size == 0) {
    return false;
  }
  assert(event->size % 4 == 0);

  uint8_t* buf = reinterpret_cast<uint8_t*>(event->dma_buf);
  auto* src = reinterpret_cast<OutputBuffers*>(user_ctx);

  BaseType_t ret1 = src->first.receive(
      {reinterpret_cast<int16_t*>(buf), event->size / 2}, false, true);
  BaseType_t ret2 = src->second.receive(
      {reinterpret_cast<int16_t*>(buf), event->size / 2}, true, true);

  // The ESP32's I2S peripheral has a different endianness to its processors.
  // ESP-IDF handles this difference for stereo channels, but not for mono
  // channels. We therefore sometimes need to swap each pair of words as they're
  // written to the DMA buffer.
  if (sSwapWords) {
    uint16_t* buf_as_words = reinterpret_cast<uint16_t*>(buf);
    for (size_t i = 0; i + 1 < event->size / 2; i += 2) {
      uint16_t temp = buf_as_words[i];
      buf_as_words[i] = buf_as_words[i + 1];
      buf_as_words[i + 1] = temp;
    }
  }

  return ret1 || ret2;
}

auto I2SDac::create(IGpios& expander, OutputBuffers& bufs)
    -> std::optional<I2SDac*> {
  i2s_chan_handle_t i2s_handle;
  i2s_chan_config_t channel_config{
      .id = kI2SPort,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 2,
      .dma_frame_num = kI2SBufferLengthFrames,
      .auto_clear = false,
      .intr_priority = 0,
  };

  ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &i2s_handle, NULL));

  // First, instantiate the instance so it can do all of its power on
  // configuration.
  std::unique_ptr<I2SDac> dac =
      std::make_unique<I2SDac>(expander, bufs, i2s_handle);

  // Whilst we wait for the initial boot, we can work on installing the I2S
  // driver.
  i2s_std_config_t i2s_config = {
      .clk_cfg = dac->clock_config_,
      .slot_cfg = dac->slot_config_,
      .gpio_cfg = {.mclk = GPIO_NUM_0,
                   .bclk = GPIO_NUM_26,
                   .ws = GPIO_NUM_27,
                   .dout = GPIO_NUM_5,
                   .din = I2S_GPIO_UNUSED,
                   .invert_flags =
                       {
                           .mclk_inv = false,
                           .bclk_inv = false,
                           .ws_inv = false,
                       }},
  };

  if (esp_err_t err =
          i2s_channel_init_std_mode(i2s_handle, &i2s_config) != ESP_OK) {
    ESP_LOGE(kTag, "failed to initialise i2s channel %x", err);
    return {};
  }

  i2s_event_callbacks_t callbacks{
      .on_recv = NULL,
      .on_recv_q_ovf = NULL,
      .on_sent = callback,
      .on_send_q_ovf = NULL,
  };
  i2s_channel_register_event_callback(i2s_handle, &callbacks, &bufs);

  return dac.release();
}

I2SDac::I2SDac(IGpios& gpio, OutputBuffers& bufs, i2s_chan_handle_t i2s_handle)
    : gpio_(gpio),
      buffers_(bufs),
      i2s_handle_(i2s_handle),
      i2s_active_(false),
      clock_config_(I2S_STD_CLK_DEFAULT_CONFIG(48000)),
      slot_config_(I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO)) {
  clock_config_.clk_src = I2S_CLK_SRC_APLL;

  // Power up the DAC.
  wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b01);

  // Reset all registers back to their default values.
  wm8523::WriteRegister(wm8523::Register::kReset, 1);

  // Wait for DAC to reset.
  vTaskDelay(pdMS_TO_TICKS(10));

  // Enable zero-cross detection and ramping for volume changes.
  wm8523::WriteRegister(wm8523::Register::kDacCtrl, 0b10011);

  // Ready to play!
  wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b10);
}

I2SDac::~I2SDac() {
  if (i2s_active_) {
    SetPaused(true);
  }

  // Power down the DAC.
  wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b01);
  wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b00);

  i2s_del_channel(i2s_handle_);
}

auto I2SDac::SetPaused(bool paused) -> void {
  if (paused) {
    wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b10);
    gpio_.WriteSync(IGpios::Pin::kAmplifierMute, true);
    set_channel(false);
  } else {
    set_channel(true);
    gpio_.WriteSync(IGpios::Pin::kAmplifierMute, false);
    wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b11);
  }
}

auto I2SDac::Reconfigure(Channels ch, BitsPerSample bps, SampleRate rate)
    -> void {
  std::lock_guard<std::mutex> lock(configure_mutex_);

  if (i2s_active_) {
    // Ramp down into mute instead of just outright stopping to minimise any
    // clicks and pops.
    wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b10);
    vTaskDelay(pdMS_TO_TICKS(1));

    wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b01);
    i2s_channel_disable(i2s_handle_);
  }

  switch (ch) {
    case CHANNELS_MONO:
      sSwapWords = true;
      slot_config_.slot_mode = I2S_SLOT_MODE_MONO;
      break;
    case CHANNELS_STEREO:
      sSwapWords = false;
      slot_config_.slot_mode = I2S_SLOT_MODE_STEREO;
      break;
  }

  uint8_t word_length = 0;
  switch (bps) {
    case BPS_16:
      slot_config_.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
      slot_config_.ws_width = 16;
      word_length = 0b00;
      break;
    case BPS_24:
      slot_config_.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT;
      slot_config_.ws_width = 24;
      word_length = 0b10;
      break;
    case BPS_32:
      slot_config_.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
      slot_config_.ws_width = 32;
      word_length = 0b11;
      break;
  }
  ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(i2s_handle_, &slot_config_));

  clock_config_.sample_rate_hz = rate;
  // If we have an MCLK/SCK, then it must be a multiple of both the sample rate
  // and the bit clock. At 24 BPS, we therefore have to change the MCLK multiple
  // to avoid issues at some sample rates. (e.g. 48KHz)
  clock_config_.mclk_multiple =
      bps == BPS_24 ? I2S_MCLK_MULTIPLE_384 : I2S_MCLK_MULTIPLE_256;
  ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(i2s_handle_, &clock_config_));

  // Set the correct word size, and set the input format to I2S-justified.
  wm8523::WriteRegister(wm8523::Register::kAifCtrl1, (word_length << 3) | 0b10);
  // Tell the DAC the clock ratio instead of waiting for it to auto detect.
  wm8523::WriteRegister(wm8523::Register::kAifCtrl2,
                        bps == BPS_24 ? 0b100 : 0b011);

  if (i2s_active_) {
    i2s_channel_enable(i2s_handle_);
    wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b11);
  } else {
    wm8523::WriteRegister(wm8523::Register::kPsCtrl, 0b10);
  }
}

auto I2SDac::WriteData(const std::span<const std::byte>& data) -> void {
  std::size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(i2s_handle_, data.data(), data.size_bytes(),
                                    &bytes_written, portMAX_DELAY);
  if (err != ESP_ERR_TIMEOUT) {
    ESP_ERROR_CHECK(err);
  }
}

auto I2SDac::set_channel(bool enabled) -> void {
  if (i2s_active_ == enabled) {
    return;
  }
  i2s_active_ = enabled;
  if (enabled) {
    i2s_channel_enable(i2s_handle_);
  } else {
    i2s_channel_disable(i2s_handle_);
  }
}

}  // namespace drivers
