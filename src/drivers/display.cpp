#include "display.hpp"

#include <cstdint>
#include <cstring>
#include <memory>

#include "assert.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/portable.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/lv_hal_disp.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"

#include "display_init.hpp"
#include "gpio_expander.hpp"

static const char* kTag = "DISPLAY";

// TODO(jacqueline): Encode width and height variations in the init data.
static const uint8_t kDisplayWidth = 128 + 2;
static const uint8_t kDisplayHeight = 160 + 1;
static const uint8_t kTransactionQueueSize = 10;

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

auto Display::create(GpioExpander* expander,
                     const displays::InitialisationData& init_data)
    -> std::unique_ptr<Display> {
  // First, turn on the LED backlight.
  expander->set_pin(GpioExpander::DISPLAY_LED, 1);
  expander->set_pin(GpioExpander::DISPLAY_POWER_ENABLE, 1);
  expander->Write();

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
      .spics_io_num = GPIO_NUM_22,
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
  display->driver_.flush_cb = &FlushDataCallback;
  display->driver_.user_data = display.get();

  ESP_LOGI(kTag, "Registering driver");
  display->display_ = lv_disp_drv_register(&display->driver_);

  return display;
}

Display::Display(GpioExpander* gpio, spi_device_handle_t handle)
    : gpio_(gpio), handle_(handle) {}

Display::~Display() {}

void Display::SendInitialisationSequence(const uint8_t* data) {
  // Reset the display manually to get it into a predictable state.
  gpio_->set_pin(GpioExpander::DISPLAY_RESET, false);
  gpio_->Write();
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_->set_pin(GpioExpander::DISPLAY_RESET, false);
  gpio_->Write();
  vTaskDelay(pdMS_TO_TICKS(10));

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

  // TODO(jacqueline): Move this to an on-board GPIO for speed.
  gpio_->set_pin(GpioExpander::DISPLAY_DR, type);
  gpio_->Write();

  // TODO(jacqueline): Handle these errors.
  esp_err_t ret = spi_device_polling_transmit(handle_, &transaction);
  ESP_ERROR_CHECK(ret);
}

void Display::OnLvglFlush(lv_disp_drv_t* disp_drv,
                          const lv_area_t* area,
                          lv_color_t* color_map) {
  // Ideally we want to complete a single flush as quickly as possible, so grab
  // the bus for this entire transaction sequence.
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
  uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
  SendCommandWithData(displays::ST77XX_RAMWR,
                      reinterpret_cast<uint8_t*>(color_map), size * 2);

  spi_device_release_bus(handle_);

  lv_disp_flush_ready(&driver_);
}

}  // namespace drivers
