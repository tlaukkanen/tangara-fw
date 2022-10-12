#include "battery.hpp"
#include "dac.hpp"
#include "gpio-expander.hpp"
#include "playback.hpp"
#include "storage.hpp"

#include <cstdint>
#include <dirent.h>
#include <memory>
#include <stdio.h>

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
      .quadwp_io_num = SPI_QUADWP_IO,
      .quadhd_io_num = SPI_QUADHD_IO,

      // Unused
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,

      // Use the DMA default size.
      .max_transfer_sz = 0,
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
      .intr_flags = 0,
  };

  ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &config, SPI_DMA_CH_AUTO));

  return ESP_OK;
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Initialising peripherals");

  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LOWMED));
  init_i2c();
  init_spi();

  ESP_LOGI(TAG, "Init GPIOs");
  gay_ipod::GpioExpander expander;

  // for debugging usb ic
  // expander.set_sd_mux(gay_ipod::GpioExpander::USB);

  ESP_LOGI(TAG, "Init ADC");
  ESP_ERROR_CHECK(gay_ipod::init_adc());

  ESP_LOGI(TAG, "Init SD card");
  auto storage_res = gay_ipod::SdStorage::create(&expander);
  if (storage_res.has_error()) {
    ESP_LOGE(TAG, "Failed: %d", storage_res.error());
    return;
  }
  std::unique_ptr<gay_ipod::SdStorage> storage = std::move(storage_res.value());

  ESP_LOGI(TAG, "Init DAC");
  auto dac_res = gay_ipod::AudioDac::create(&expander);
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

  ESP_LOGI(TAG, "Everything looks good! Waiting a mo for debugger.");
  vTaskDelay(pdMS_TO_TICKS(1500));

  playback->Play("/sdcard/test.mp3");
  playback->set_volume(100);

  playback->WaitForSongEnd();

  ESP_LOGI(TAG, "Time to deinit.");

  ESP_LOGI(TAG, "Hooray!");
}
