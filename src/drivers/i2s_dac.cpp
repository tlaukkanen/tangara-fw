/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "i2s_dac.hpp"
#include <sys/unistd.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "assert.h"
#include "driver/i2c.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "gpios.hpp"
#include "hal/i2s_types.h"
#include "i2c.hpp"
#include "soc/clk_tree_defs.h"
#include "sys/_stdint.h"

namespace drivers {

static const char* kTag = "i2s_dac";
static const i2s_port_t kI2SPort = I2S_NUM_0;
static const uint8_t kWm8523Address = 0b0011010;

enum Register {
  kReset = 0,
  kRevision = 1,
  kPsCtrl = 2,
  kAifCtrl1 = 3,
  kAifCtrl2 = 4,
  kDacCtrl = 5,
  kDacGainLeft = 6,
  kDacGainRight = 7,
  kZeroDetect = 8,
};

auto write_register(Register reg, uint8_t msb, uint8_t lsb) -> esp_err_t {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kWm8523Address, I2C_MASTER_WRITE)
      .write_ack(reg, msb, lsb)
      .stop();
  return transaction.Execute();
}

auto I2SDac::create(IGpios* expander) -> std::optional<I2SDac*> {
  i2s_chan_handle_t i2s_handle;
  i2s_chan_config_t channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(kI2SPort, I2S_ROLE_MASTER);

  // Use the maximum possible DMA buffer size, since a smaller number of large
  // copies is faster than a large number of small copies.
  channel_config.dma_frame_num = 1024;
  // Triple buffering should be enough to keep samples flowing smoothly.
  // TODO(jacqueline): verify this with 192kHz 32bps.
  channel_config.dma_desc_num = 4;
  // channel_config.auto_clear = true;

  ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &i2s_handle, NULL));
  //
  // First, instantiate the instance so it can do all of its power on
  // configuration.
  std::unique_ptr<I2SDac> dac = std::make_unique<I2SDac>(expander, i2s_handle);

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
                           .ws_inv = true,
                       }},
  };

  if (esp_err_t err =
          i2s_channel_init_std_mode(i2s_handle, &i2s_config) != ESP_OK) {
    ESP_LOGE(kTag, "failed to initialise i2s channel %x", err);
    return {};
  }

  return dac.release();
}

I2SDac::I2SDac(IGpios* gpio, i2s_chan_handle_t i2s_handle)
    : gpio_(gpio),
      i2s_handle_(i2s_handle),
      i2s_active_(false),
      active_page_(),
      clock_config_(I2S_STD_CLK_DEFAULT_CONFIG(44100)),
      slot_config_(I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                   I2S_SLOT_MODE_STEREO)) {
  clock_config_.clk_src = I2S_CLK_SRC_PLL_160M;

  // Keep the 5V circuity off until it's needed.
  gpio_->WriteSync(IGpios::Pin::kAmplifierEnable, false);

  // Reset all registers back to their default values.
  write_register(kReset, 0, 1);
  vTaskDelay(pdMS_TO_TICKS(10));

  // Power up the charge pump.
  write_register(kPsCtrl, 0, 0b01);
}

I2SDac::~I2SDac() {
  Stop();
  i2s_del_channel(i2s_handle_);
}

auto I2SDac::Start() -> void {
  gpio_->WriteSync(IGpios::Pin::kAmplifierEnable, true);

  i2s_channel_enable(i2s_handle_);
  write_register(kPsCtrl, 0, 0b11);

  i2s_active_ = true;
}

auto I2SDac::Stop() -> void {
  write_register(kPsCtrl, 0, 0b01);
  i2s_channel_disable(i2s_handle_);

  gpio_->WriteSync(IGpios::Pin::kAmplifierEnable, false);

  i2s_active_ = false;
}

auto I2SDac::Reconfigure(Channels ch, BitsPerSample bps, SampleRate rate)
    -> void {
  if (i2s_active_) {
    write_register(kPsCtrl, 0, 0b01);
    i2s_channel_disable(i2s_handle_);
  }

  switch (ch) {
    case CHANNELS_MONO:
      slot_config_.slot_mode = I2S_SLOT_MODE_MONO;
      break;
    case CHANNELS_STEREO:
      slot_config_.slot_mode = I2S_SLOT_MODE_STEREO;
      break;
  }

  uint8_t word_length = 0;
  switch (bps) {
    case BPS_16:
      slot_config_.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
      word_length = 0b00;
      break;
    case BPS_24:
      slot_config_.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT;
      word_length = 0b10;
      break;
    case BPS_32:
      slot_config_.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
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
  write_register(kAifCtrl1, 0, (word_length << 3) & 0b10);
  // Tell the DAC the clock ratio instead of waiting for it to auto detect.
  write_register(kAifCtrl2, 0, bps == BPS_24 ?  0b100 : 0b011);

  if (i2s_active_) {
    i2s_channel_enable(i2s_handle_);
    write_register(kPsCtrl, 0, 0b11);
  }
}

auto I2SDac::WriteData(const cpp::span<const std::byte>& data) -> void {
  std::size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(i2s_handle_, data.data(), data.size_bytes(),
                                    &bytes_written, portMAX_DELAY);
  if (err != ESP_ERR_TIMEOUT) {
    ESP_ERROR_CHECK(err);
  }
}

static constexpr double increment = (2.0 * 3.141592) / (44100.0 / 500.0);
static constexpr double amplitude = 16'777'216.0 * 0.6;
static double current = 0;
static uint8_t leftover = 0;
static bool left = false;

extern "C" IRAM_ATTR auto callback(i2s_chan_handle_t handle,
                                   i2s_event_data_t* event,
                                   void* user_ctx) -> bool {
  if (event == nullptr || user_ctx == nullptr) {
    return false;
  }
  if (event->data == nullptr || event->size == 0) {
    return false;
  }
  /*
  uint8_t** buf = reinterpret_cast<uint8_t**>(event->data);
  StreamBufferHandle_t src = reinterpret_cast<StreamBufferHandle_t>(user_ctx);
  BaseType_t ret = false;
  std::size_t bytes_received =
      xStreamBufferReceiveFromISR(src, *buf, event->size, &ret);
  if (bytes_received < event->size) {
    memset(*buf + bytes_received, 0, event->size - bytes_received);
  }
  return ret;
  */
  uint8_t* buf = *(reinterpret_cast<uint8_t**>(event->data));
  std::size_t i = 0;
  while (i < event->size) {
    uint32_t sample = amplitude * std::sin(current);
    if (leftover > 0) {
      if (leftover == 2) {
        buf[i++] = (sample >> 8) & 0xFF;
        leftover--;
      }
      if (leftover == 1) {
        buf[i++] = sample & 0xFF;
        leftover--;
      }
      continue;
    }

    buf[i++] = (sample >> 16) & 0xFF;
    if (i == event->size) {
      leftover = 2;
      return false;
    }
    buf[i++] = (sample >> 8) & 0xFF;
    if (i == event->size) {
      leftover = 1;
      return false;
    }
    buf[i++] = sample & 0xFF;
    if (left) {
      current += increment;
      left = false;
    } else {
      left = true;
    }
  }
  return false;
}

auto I2SDac::SetSource(StreamBufferHandle_t buffer) -> void {
  if (i2s_active_) {
    ESP_ERROR_CHECK(i2s_channel_disable(i2s_handle_));
  }
  i2s_event_callbacks_t callbacks{
      .on_recv = NULL,
      .on_recv_q_ovf = NULL,
      .on_sent = NULL,
      .on_send_q_ovf = NULL,
  };
  if (buffer != nullptr) {
    callbacks.on_sent = &callback;
  }
  i2s_channel_register_event_callback(i2s_handle_, &callbacks, buffer);
  if (i2s_active_) {
    ESP_ERROR_CHECK(i2s_channel_enable(i2s_handle_));
  }
}

}  // namespace drivers
