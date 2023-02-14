#include "display.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>

#include "assert.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "freertos/portable.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"

#include "display_init.hpp"

static const char* kTag = "DISPLAY";

static const uint8_t kDisplayWidth = 128;
static const uint8_t kDisplayHeight = 160;
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
// it's not a memory hit we can avoid anyway.
// Note: 128 * 160 / 10 * 2 bpp * 2 buffers = 8 KiB
DMA_ATTR static lv_color_t sBuffer1[kDisplayBufferSize];
DMA_ATTR static lv_color_t sBuffer2[kDisplayBufferSize];

namespace drivers {

// Static functions for interrop with the LVGL display driver API, which
// requires a function pointer.
namespace callback {
static std::atomic<Display*> instance = nullptr;

extern "C" void flush_cb(lv_disp_drv_t* disp_drv,
                         const lv_area_t* area,
                         lv_color_t* color_map) {
  auto instance_unwrapped = instance.load();
  if (instance_unwrapped == nullptr) {
    ESP_LOGW(kTag, "uncaught flush callback");
    return;
  }
  // TODO: what if a transaction comes in right now?
  instance_unwrapped->Flush(disp_drv, area, color_map);
}

static void IRAM_ATTR post_cb(spi_transaction_t* transaction) {
  auto instance_unwrapped = instance.load();
  if (instance_unwrapped == nullptr) {
    // Can't log in ISR.
    return;
  }
  instance_unwrapped->PostTransaction(*transaction);
}
}  // namespace callback

auto Display::create(GpioExpander* expander,
                     const displays::InitialisationData& init_data)
    -> cpp::result<std::unique_ptr<Display>, Error> {
  expander->set_pin(GpioExpander::DISPLAY_LED, 0);
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
      .post_cb = &callback::post_cb,
  };
  spi_device_handle_t handle;
  spi_bus_add_device(VSPI_HOST, &spi_cfg, &handle);

  // TODO: ideally create this later? a bit awkward rn.
  auto display = std::make_unique<Display>(expander, handle);

  // Now we reset the display into a known state, then configure it
  // TODO: set rotatoin
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
  display->driver_.flush_cb = &callback::flush_cb;

  ESP_LOGI(kTag, "Registering driver");
  display->display_ = lv_disp_drv_register(&display->driver_);

  return display;
}

Display::Display(GpioExpander* gpio, spi_device_handle_t handle)
    : gpio_(gpio), handle_(handle) {
  callback::instance = this;
}

Display::~Display() {
  callback::instance = nullptr;
  // TODO.
}

void Display::SendInitialisationSequence(const uint8_t* data) {
  uint8_t command, num_args;
  uint16_t sleep_duration_ms;

  // First byte of the data is the number of commands.
  for (int i = *(data++); i > 0; i--) {
    command = *(data++);
    num_args = *(data++);
    bool has_delay = (num_args & displays::kDelayBit) > 0;
    num_args &= ~displays::kDelayBit;

    SendCommandWithData(command, data, num_args);

    data += num_args;
    if (has_delay) {
      sleep_duration_ms = *(data++);
      if (sleep_duration_ms == 0xFF) {
        sleep_duration_ms = 500;
      }
      vTaskDelay(pdMS_TO_TICKS(sleep_duration_ms));
    }
  }
}

void Display::SendCommandWithData(uint8_t command,
                                  const uint8_t* data,
                                  size_t length,
                                  uintptr_t flags) {
  SendCmd(&command, 1, flags);
  SendData(data, length, flags);
}

void Display::SendCmd(const uint8_t* data, size_t length, uintptr_t flags) {
  SendTransaction(COMMAND, data, length, flags);
}

void Display::SendData(const uint8_t* data, size_t length, uintptr_t flags) {
  SendTransaction(DATA, data, length, flags);
}

void Display::SendTransaction(TransactionType type,
                              const uint8_t* data,
                              size_t length,
                              uint32_t flags) {
  if (length == 0) {
    return;
  }

  // TODO: Use a memory pool for these.
  spi_transaction_t* transaction = (spi_transaction_t*)heap_caps_calloc(
      1, sizeof(spi_transaction_t), MALLOC_CAP_DMA);

  transaction->rx_buffer = NULL;
  // Length is in bits, so multiply by 8.
  transaction->length = length * 8;
  transaction->rxlength = 0;  // Match `length` value.

  // If the data to transmit is very short, then we can fit it directly
  // inside the transaction struct.
  if (length * 8 <= 32) {
    transaction->flags = SPI_TRANS_USE_TXDATA;
    std::memcpy(&transaction->tx_data, data, length);
  } else {
    // TODO: copy data to a DMA-capable transaction buffer
    transaction->tx_buffer = const_cast<uint8_t*>(data);
  }

  transaction->user = reinterpret_cast<void*>(flags);

  // TODO: acquire the bus first? Or in an outer scope?
  // TODO: fail gracefully
  // ESP_ERROR_CHECK(spi_device_queue_trans(handle_, transaction,
  // portMAX_DELAY));
  //

  ServiceTransactions();

  gpio_->with(
      [&](auto& gpio) { gpio.set_pin(GpioExpander::DISPLAY_DR, type); });

  ESP_ERROR_CHECK(spi_device_polling_transmit(handle_, transaction));

  free(transaction);
}

void Display::Flush(lv_disp_drv_t* disp_drv,
                    const lv_area_t* area,
                    lv_color_t* color_map) {
  uint16_t data[2] = {0, 0};

  data[0] = SPI_SWAP_DATA_TX(area->x1, 16);
  data[1] = SPI_SWAP_DATA_TX(area->x2, 16);
  SendCommandWithData(displays::ST77XX_CASET, (uint8_t*)data, 4);

  data[0] = SPI_SWAP_DATA_TX(area->y1, 16);
  data[1] = SPI_SWAP_DATA_TX(area->y2, 16);
  SendCommandWithData(displays::ST77XX_RASET, (uint8_t*)data, 4);

  uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
  SendCommandWithData(displays::ST77XX_RAMWR, (uint8_t*)color_map, size * 2,
                      LVGL_FLUSH);

  // ESP_LOGI(kTag, "finished flush.");
  // lv_disp_flush_ready(&driver_);
}

void Display::PostTransaction(const spi_transaction_t& transaction) {
  if (reinterpret_cast<uintptr_t>(transaction.user) & LVGL_FLUSH) {
    lv_disp_flush_ready(&driver_);
  }
}

void Display::ServiceTransactions() {
  // todo
  if (1)
    return;
  spi_transaction_t* transaction = nullptr;
  // TODO: just wait '1' here, provide mechanism to wait for sure (poll?)
  while (spi_device_get_trans_result(handle_, &transaction, pdMS_TO_TICKS(1)) !=
         ESP_ERR_TIMEOUT) {
    ESP_LOGI(kTag, "cleaning up finished transaction");

    // TODO: a bit dodge lmao
    // TODO: also this should happen in the post callback instead i guess?
    if (transaction->length > 1000) {
      ESP_LOGI(kTag, "finished flush.");
      lv_disp_flush_ready(&driver_);
    }

    // TODO: place back into pool.
    free(transaction);
  }
}

}  // namespace drivers
