/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "display.hpp"

#include <cstdint>
#include <cstring>
#include <memory>

#include "assert.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/portable.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/ledc_types.h"
#include "hal/lv_hal_disp.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"

#include "display_init.hpp"
#include "gpio_expander.hpp"
#include "soc/soc.h"
#include "tasks.hpp"

static const char* kTag = "DISPLAY";

// TODO(jacqueline): Encode width and height variations in the init data.
static const uint8_t kDisplayWidth = 128 + 2;
static const uint8_t kDisplayHeight = 160 + 1;
static const uint8_t kTransactionQueueSize = 10;

static const gpio_num_t kDisplayDr = GPIO_NUM_33;
static const gpio_num_t kDisplayLedEn = GPIO_NUM_32;
static const gpio_num_t kDisplayCs = GPIO_NUM_22;

/*
 * The size of each of our two display buffers. This is fundamentally a balance
 * between performance and memory usage. LVGL docs recommend a buffer 1/10th the
 * size of the screen is the best tradeoff.
 * We use two buffers so that one can be flushed to the screen at the same time
 * as the other is being drawn.
 */
static const int kDisplayBufferSize = (kDisplayWidth * kDisplayHeight) / 10;

// Allocate both buffers in static memory to ensure that they're in DRAM, with
// minimal fragmentation. We most cases we always need these buffers anyway, so
// it's not a memory hit we can avoid.
// Note: 128 * 160 / 10 * 2 bpp * 2 buffers = 8 KiB
DMA_ATTR static lv_color_t sBuffer1[kDisplayBufferSize];
DMA_ATTR static lv_color_t sBuffer2[kDisplayBufferSize];

struct RenderTaskArgs {
  std::atomic<bool>* quit;
  QueueHandle_t work_queue;
};

struct FlushArgs {
  lv_disp_drv_t* driver;
  const lv_area_t* area;
  lv_color_t* color_map;
};

void RenderMain(void* raw_args);

namespace drivers {

/*
 * Callback invoked by LVGL when there is new data to be written to the display.
 */
extern "C" void FlushDataCallback(lv_disp_drv_t* disp_drv,
                                  const lv_area_t* area,
                                  lv_color_t* color_map) {
  Display* instance = static_cast<Display*>(disp_drv->user_data);
  instance->OnLvglFlush(disp_drv, area, color_map);
}

auto Display::Create(GpioExpander* expander,
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
      .duty_resolution = LEDC_TIMER_13_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 5000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ledc_timer_config(&led_config);

  ledc_channel_config_t led_channel{.gpio_num = kDisplayLedEn,
                                    .speed_mode = LEDC_LOW_SPEED_MODE,
                                    .channel = LEDC_CHANNEL_0,
                                    .timer_sel = LEDC_TIMER_0,
                                    .duty = 4095,
                                    .hpoint = 0};
  ledc_channel_config(&led_channel);

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
  // TODO(jacqueline): set rotation
  ESP_LOGI(kTag, "Sending init sequences");
  for (int i = 0; i < init_data.num_sequences; i++) {
    display->SendInitialisationSequence(init_data.sequences[i]);
  }

  // The hardware is now configured correctly. Next, initialise the LVGL display
  // driver.
  ESP_LOGI(kTag, "Init buffers");
  lv_disp_draw_buf_init(&display->buffers_, sBuffer1, sBuffer2,
                        kDisplayBufferSize);
  lv_disp_drv_init(&display->driver_);
  display->driver_.draw_buf = &display->buffers_;
  display->driver_.hor_res = kDisplayWidth;
  display->driver_.ver_res = kDisplayHeight;
  display->driver_.sw_rotate = 1;
  display->driver_.rotated = LV_DISP_ROT_270;
  display->driver_.antialiasing = 0;
  display->driver_.flush_cb = &FlushDataCallback;
  display->driver_.user_data = display.get();

  ESP_LOGI(kTag, "Registering driver");
  display->display_ = lv_disp_drv_register(&display->driver_);

  return display.release();
}

Display::Display(GpioExpander* gpio, spi_device_handle_t handle)
    : gpio_(gpio),
      handle_(handle),
      worker_task_(tasks::Worker::Start<tasks::Type::kUiFlush>()) {}

Display::~Display() {}

void Display::SendInitialisationSequence(const uint8_t* data) {
  // Hold onto the bus for the entire sequence so that we're not interrupted
  // part way through.
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

      // Avoid hanging on to the bus whilst delaying.
      spi_device_release_bus(handle_);
      vTaskDelay(pdMS_TO_TICKS(sleep_duration_ms));
      spi_device_acquire_bus(handle_, portMAX_DELAY);
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

  spi_transaction_t transaction;
  memset(&transaction, 0, sizeof(transaction));

  transaction.rx_buffer = NULL;
  // Length is in bits, so multiply by 8.
  transaction.length = length * 8;
  transaction.rxlength = 0;  // Match `length` value.

  // If the data to transmit is very short, then we can fit it directly
  // inside the transaction struct.
  if (transaction.length <= 32) {
    transaction.flags = SPI_TRANS_USE_TXDATA;
    std::memcpy(&transaction.tx_data, data, length);
  } else {
    // Note: LVGL's buffers are in DMA-accessible memory, so whatever pointer
    // it handed us should be DMA-accessible already. No need to copy.
    transaction.tx_buffer = data;
  }

  gpio_set_level(kDisplayDr, type);

  // TODO(jacqueline): Handle these errors.
  esp_err_t ret = spi_device_polling_transmit(handle_, &transaction);
  ESP_ERROR_CHECK(ret);
}

void Display::OnLvglFlush(lv_disp_drv_t* disp_drv,
                          const lv_area_t* area,
                          lv_color_t* color_map) {
  // area is stack-allocated, so it isn't safe to reference from the flush
  // thread.
  lv_area_t area_copy = *area;
  // worker_task_->Dispatch<void>([=, this]() {
  //  Ideally we want to complete a single flush as quickly as possible, so
  //  grab the bus for this entire transaction sequence.
  spi_device_acquire_bus(handle_, portMAX_DELAY);

  // First we need to specify the rectangle of the display we're writing into.
  uint16_t data[2] = {0, 0};

  data[0] = SPI_SWAP_DATA_TX(area_copy.x1, 16);
  data[1] = SPI_SWAP_DATA_TX(area_copy.x2, 16);
  SendCommandWithData(displays::ST77XX_CASET, reinterpret_cast<uint8_t*>(data),
                      4);

  data[0] = SPI_SWAP_DATA_TX(area_copy.y1, 16);
  data[1] = SPI_SWAP_DATA_TX(area_copy.y2, 16);
  SendCommandWithData(displays::ST77XX_RASET, reinterpret_cast<uint8_t*>(data),
                      4);

  // Now send the pixels for this region.
  uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
  SendCommandWithData(displays::ST77XX_RAMWR,
                      reinterpret_cast<uint8_t*>(color_map), size * 2);

  spi_device_release_bus(handle_);

  lv_disp_flush_ready(&driver_);
  //});
}

void RenderMain(void* raw_args) {
  RenderTaskArgs* args = reinterpret_cast<RenderTaskArgs*>(raw_args);
  QueueHandle_t queue = args->work_queue;
  std::atomic<bool>* quit = args->quit;
  delete args;

  while (!quit->load()) {
    // TODO: flush data here! Yay speed.
  }

  vQueueDelete(queue);
  delete quit;
  vTaskDelete(NULL);
}

}  // namespace drivers
