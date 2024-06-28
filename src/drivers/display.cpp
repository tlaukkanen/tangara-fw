/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/display.hpp"
#include <stdint.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include "assert.h"
#include "display/lv_display.h"
#include "draw/sw/lv_draw_sw.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_memory_utils.h"
#include "freertos/portable.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/ledc_types.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"

#include "drivers/display_init.hpp"
#include "drivers/gpios.hpp"
#include "drivers/spi.hpp"
#include "misc/lv_color.h"
#include "soc/soc.h"
#include "tasks.hpp"

[[maybe_unused]] static const char* kTag = "DISPLAY";

static const uint8_t kTransactionQueueSize = 2;

static const gpio_num_t kDisplayDr = GPIO_NUM_33;
static const gpio_num_t kDisplayLedEn = GPIO_NUM_32;
static const gpio_num_t kDisplayCs = GPIO_NUM_22;

/*
 * The size of each of our two display buffers. This is fundamentally a balance
 * between performance and memory usage. LVGL docs recommend a buffer 1/10th the
 * size of the screen is the best tradeoff.
 8
 * The 160x128 is the nominal size of our standard faceplate's display.
 */
static const int kDisplayBufferSize = 160 * 128 / 10;
DMA_ATTR static lv_color_t kDisplayBuffer[kDisplayBufferSize];

namespace drivers {

/*
 * Callback invoked by LVGL when there is new data to be written to the display.
 */
extern "C" void FlushDataCallback(lv_display_t* display,
                                  const lv_area_t* area,
                                  uint8_t* px_map) {
  Display* instance = static_cast<Display*>(lv_display_get_user_data(display));
  instance->OnLvglFlush(area, px_map);
}

auto Display::Create(IGpios& expander,
                     const displays::InitialisationData& init_data)
    -> Display* {
  ESP_LOGI(kTag, "Init I/O pins");
  gpio_config_t dr_config{
      .pin_bit_mask = 1ULL << kDisplayDr,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&dr_config);
  gpio_set_level(kDisplayDr, 0);

  ledc_timer_config_t led_config{
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_10_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 50000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&led_config));

  gpio_config_t led_pin_config{
      .pin_bit_mask = 1ULL << kDisplayLedEn,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&led_pin_config);

  ledc_channel_config_t led_channel{.gpio_num = kDisplayLedEn,
                                    .speed_mode = LEDC_LOW_SPEED_MODE,
                                    .channel = LEDC_CHANNEL_0,
                                    .timer_sel = LEDC_TIMER_0,
                                    .duty = 0,
                                    .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&led_channel));

  ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

  ledc_fade_func_install(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_SHARED |
                         ESP_INTR_FLAG_IRAM);

  // Next, init the SPI device
  spi_device_interface_config_t spi_cfg = {
      .command_bits = 0,  // No command phase
      .address_bits = 0,  // No address phase
      .dummy_bits = 0,
      // For ST7789, mode should be 2
      .mode = 0,
      .duty_cycle_pos = 0,  // Unused
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .clock_speed_hz = SPI_MASTER_FREQ_40M,
      .input_delay_ns = 0,
      .spics_io_num = kDisplayCs,
      .flags = 0,
      .queue_size = kTransactionQueueSize,
      .pre_cb = NULL,
      .post_cb = NULL,
  };
  spi_device_handle_t handle;
  spi_bus_add_device(VSPI_HOST, &spi_cfg, &handle);

  auto display = std::make_unique<Display>(expander, handle);

  // Now we reset the display into a known state, then configure it
  ESP_LOGI(kTag, "Sending init sequences");
  for (int i = 0; i < init_data.num_sequences; i++) {
    display->SendInitialisationSequence(init_data.sequences[i]);
  }

  // The hardware is now configured correctly. Next, initialise the LVGL display
  // driver.
  ESP_LOGI(kTag, "Init buffers");
  assert(esp_ptr_dma_capable(kDisplayBuffer));

  ESP_LOGI(kTag, "Creating display");
  display->display_ = lv_display_create(init_data.width, init_data.height);
  lv_display_set_buffers(display->display_, kDisplayBuffer, NULL,
                         sizeof(kDisplayBuffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(display->display_, LV_COLOR_FORMAT_RGB565);
  lv_display_set_user_data(display->display_, display.get());
  lv_display_set_flush_cb(display->display_, &FlushDataCallback);
  lv_display_set_default(display->display_);

  return display.release();
}

Display::Display(IGpios& gpio, spi_device_handle_t handle)
    : gpio_(gpio),
      handle_(handle),
      first_flush_finished_(false),
      display_on_(false),
      brightness_(0) {}

Display::~Display() {
  ledc_fade_func_uninstall();
}

auto Display::SetDisplayOn(bool enabled) -> void {
  display_on_ = enabled;
  if (!first_flush_finished_) {
    return;
  }

  if (display_on_) {
    SendCommandWithData(displays::ST77XX_DISPON, nullptr, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  int new_duty = display_on_ ? brightness_ : 0;
  SetDutyCycle(new_duty, true);

  if (!display_on_) {
    vTaskDelay(pdMS_TO_TICKS(100));
    SendCommandWithData(displays::ST77XX_DISPOFF, nullptr, 0);
  }
}

auto Display::SetBrightness(uint_fast8_t percent) -> void {
  brightness_ =
      std::pow(static_cast<double>(percent) / 100.0, 2.8) * 1024.0 + 0.5;
  if (first_flush_finished_ && display_on_) {
    SetDutyCycle(brightness_, false);
  }
}

auto Display::SetDutyCycle(uint_fast8_t new_duty, bool fade) -> void {
  if (fade) {
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, new_duty, 100);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
  } else {
    ESP_ERROR_CHECK(
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, new_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
  }
}

void Display::SendInitialisationSequence(const uint8_t* data) {
  // Hold the SPI bus for the entire init sequence, as otherwise SD init may
  // grab it and delay showing the boot splash. The total time until boot is
  // finished may be increased by doing this, but a short boot with no feedback
  // feels worse than a longer boot that doesn't tell you anything.
  spi_device_acquire_bus(handle_, portMAX_DELAY);

  // First byte of the data is the number of commands.
  for (int i = *(data++); i > 0; i--) {
    uint8_t command = *(data++);
    uint8_t num_args = *(data++);
    bool has_delay = (num_args & displays::kDelayBit) > 0;
    num_args &= ~displays::kDelayBit;

    SendCommandWithData(command, data, num_args);

    data += num_args;
    if (has_delay) {
      uint16_t sleep_duration_ms = *(data++);
      if (sleep_duration_ms == 0xFF) {
        sleep_duration_ms = 500;
      }

      vTaskDelay(pdMS_TO_TICKS(sleep_duration_ms));
    }
  }

  spi_device_release_bus(handle_);
}

void Display::SendCommandWithData(uint8_t command,
                                  const uint8_t* data,
                                  size_t length) {
  SendCmd(&command, 1);
  SendData(data, length);
}

void Display::SendCmd(const uint8_t* data, size_t length) {
  SendTransaction(COMMAND, data, length);
}

void Display::SendData(const uint8_t* data, size_t length) {
  SendTransaction(DATA, data, length);
}

void Display::SendTransaction(TransactionType type,
                              const uint8_t* data,
                              size_t length) {
  // TODO(jacqueline): What's sending this?
  if (length == 0) {
    return;
  }

  DMA_ATTR static spi_transaction_t sTransaction;
  memset(&sTransaction, 0, sizeof(sTransaction));

  sTransaction.rx_buffer = NULL;
  // Length is in bits, so multiply by 8.
  sTransaction.length = length * 8;
  sTransaction.rxlength = 0;  // Match `length` value.

  // If the data to transmit is very short, then we can fit it directly
  // inside the transaction struct.
  if (sTransaction.length <= 32) {
    sTransaction.flags = SPI_TRANS_USE_TXDATA;
    std::memcpy(&sTransaction.tx_data, data, length);
  } else {
    // Note: LVGL's buffers are in DMA-accessible memory, so whatever pointer
    // it handed us should be DMA-accessible already. No need to copy.
    sTransaction.tx_buffer = data;
  }

  gpio_set_level(kDisplayDr, type);

  // TODO(jacqueline): Handle these errors better.
  ESP_ERROR_CHECK(spi_device_transmit(handle_, &sTransaction));
}

void Display::OnLvglFlush(const lv_area_t* area, uint8_t* color_map) {
  // Swap the pixel byte order first, since we don't want to do this whilst
  // holding the SPI bus lock.
  uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(color_map, size);

  spi_device_acquire_bus(handle_, portMAX_DELAY);

  // First we need to specify the rectangle of the display we're writing into.
  uint16_t data[2] = {0, 0};

  data[0] = SPI_SWAP_DATA_TX(area->x1, 16);
  data[1] = SPI_SWAP_DATA_TX(area->x2, 16);
  SendCommandWithData(displays::ST77XX_CASET, reinterpret_cast<uint8_t*>(data),
                      4);

  data[0] = SPI_SWAP_DATA_TX(area->y1, 16);
  data[1] = SPI_SWAP_DATA_TX(area->y2, 16);
  SendCommandWithData(displays::ST77XX_RASET, reinterpret_cast<uint8_t*>(data),
                      4);

  // Now send the pixels for this region.
  SendCommandWithData(displays::ST77XX_RAMWR,
                      reinterpret_cast<uint8_t*>(color_map), size * 2);

  spi_device_release_bus(handle_);

  if (!first_flush_finished_ && lv_disp_flush_is_last(display_)) {
    first_flush_finished_ = true;
    SetDisplayOn(display_on_);
  }

  lv_display_flush_ready(display_);
}

}  // namespace drivers
