#include "battery.hpp"
#include "core/lv_disp.h"
#include "core/lv_obj_pos.h"
#include "dac.hpp"
#include "display-init.hpp"
#include "display.hpp"
#include "esp_freertos_hooks.h"
#include "freertos/portmacro.h"
#include "gpio-expander.hpp"
#include "misc/lv_color.h"
#include "misc/lv_timer.h"
#include "playback.hpp"
#include "storage.hpp"

#include <dirent.h>
#include <stdio.h>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"
#include "widgets/lv_label.h"

#define I2C_SDA_IO (GPIO_NUM_2)
#define I2C_SCL_IO (GPIO_NUM_4)
#define I2C_CLOCK_HZ (400000)

#define SPI_SDI_IO (GPIO_NUM_19)
#define SPI_SDO_IO (GPIO_NUM_23)
#define SPI_SCLK_IO (GPIO_NUM_18)
#define SPI_QUADWP_IO (GPIO_NUM_22)
#define SPI_QUADHD_IO (GPIO_NUM_21)

static const char* TAG = "MAIN";

esp_err_t init_i2c(void) {
  i2c_port_t port = I2C_NUM_0;
  i2c_config_t config = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_SDA_IO,
      .scl_io_num = I2C_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master =
          {
              .clk_speed = I2C_CLOCK_HZ,
          },
      // No requirements for the clock.
      .clk_flags = 0,
  };

  ESP_ERROR_CHECK(i2c_param_config(port, &config));
  ESP_ERROR_CHECK(i2c_driver_install(port, config.mode, 0, 0, 0));

  // TODO: INT line

  return ESP_OK;
}

esp_err_t init_spi(void) {
  spi_bus_config_t config = {
      .mosi_io_num = SPI_SDO_IO,
      .miso_io_num = SPI_SDI_IO,
      .sclk_io_num = SPI_SCLK_IO,
      .quadwp_io_num = -1,  // SPI_QUADWP_IO,
      .quadhd_io_num = -1,  // SPI_QUADHD_IO,

      // Unused
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,

      // Use the DMA default size. The display requires larger buffers, but it
      // manages its down use of DMA-capable memory.
      .max_transfer_sz = 128 * 16 * 2,  // TODO: hmm
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
      .intr_flags = 0,
  };

  ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &config, SPI_DMA_CH_AUTO));

  return ESP_OK;
}

void IRAM_ATTR tick_hook(void) {
  lv_tick_inc(1);
}

static const size_t kLvglStackSize = 8 * 1024;
static StaticTask_t sLvglTaskBuffer = {};
static StackType_t sLvglStack[kLvglStackSize] = {0};

struct LvglArgs {
  gay_ipod::GpioExpander* gpio_expander;
};

void lvgl_main(void* voidArgs) {
  ESP_LOGI(TAG, "starting LVGL task");
  LvglArgs* args = (LvglArgs*)voidArgs;
  gay_ipod::GpioExpander* gpio_expander = args->gpio_expander;

  // Dispose of the args now that we've gotten everything out of them.
  delete args;

  ESP_LOGI(TAG, "init lvgl");
  lv_init();

  // LVGL has been initialised, so we can now start reporting ticks to it.
  esp_register_freertos_tick_hook(&tick_hook);

  ESP_LOGI(TAG, "init display");
  auto display_res =
      gay_ipod::Display::create(gpio_expander, gay_ipod::displays::kST7735R);
  if (display_res.has_error()) {
    ESP_LOGE(TAG, "Failed: %d", display_res.error());
    return;
  }
  std::unique_ptr<gay_ipod::Display> display = std::move(display_res.value());

  auto label = lv_label_create(NULL);
  lv_label_set_text(label, "g'day, cunts!");
  lv_obj_center(label);
  lv_scr_load(label);

  while (1) {
    lv_timer_handler();
    // display->ServiceTransactions();
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // TODO: break from the loop to kill this task, so that we can do our RAII
  // cleanup, unregister our tick callback and so on.
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Initialising peripherals");

  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LOWMED));
  init_i2c();
  init_spi();
  ESP_ERROR_CHECK(gay_ipod::init_adc());

  ESP_LOGI(TAG, "Init GPIOs");
  gay_ipod::GpioExpander* expander = new gay_ipod::GpioExpander();

  // for debugging usb ic
  // expander.set_sd_mux(gay_ipod::GpioExpander::USB);

  /*
  ESP_LOGI(TAG, "Init SD card");
  auto storage_res = gay_ipod::SdStorage::create(expander);
  if (storage_res.has_error()) {
    ESP_LOGE(TAG, "Failed: %d", storage_res.error());
    return;
  }
  std::unique_ptr<gay_ipod::SdStorage> storage = std::move(storage_res.value());

  ESP_LOGI(TAG, "Init DAC");
  auto dac_res = gay_ipod::AudioDac::create(expander);
  if (storage_res.has_error()) {
    ESP_LOGE(TAG, "Failed: %d", dac_res.error());
    return;
  }
  std::unique_ptr<gay_ipod::AudioDac> dac = std::move(dac_res.value());

  ESP_LOGI(TAG, "Init Audio Pipeline");
  auto playback_res = gay_ipod::DacAudioPlayback::create(dac.get());
  if (playback_res.has_error()) {
    ESP_LOGE(TAG, "Failed: %d", playback_res.error());
    return;
  }
  std::unique_ptr<gay_ipod::DacAudioPlayback> playback =
      std::move(playback_res.value());
  */

  ESP_LOGI(TAG, "Everything looks good! Waiting a mo for debugger.");
  vTaskDelay(pdMS_TO_TICKS(1500));

  LvglArgs* lvglArgs = (LvglArgs*)calloc(1, sizeof(LvglArgs));
  lvglArgs->gpio_expander = expander;
  xTaskCreateStaticPinnedToCore(&lvgl_main, "LVGL", kLvglStackSize, (void*)lvglArgs,
                                1, sLvglStack, &sLvglTaskBuffer, 1);

  while (1) {
    // TODO: Find owners for everything so we can quit this task safely.
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
