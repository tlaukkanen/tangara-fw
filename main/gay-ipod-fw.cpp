#include <stdio.h>

#include "battery.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_adc_cal.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "gpio-expander.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "storage.h"

#define I2C_SDA_IO (GPIO_NUM_0)
#define I2C_SCL_IO (GPIO_NUM_4)
#define I2C_CLOCK_HZ (400000)

#define SPI_SDI_IO (GPIO_NUM_19)
#define SPI_SDO_IO (GPIO_NUM_23)
#define SPI_SCLK_IO (GPIO_NUM_18)
#define SPI_QUADWP_IO (GPIO_NUM_22)
#define SPI_QUADHD_IO (GPIO_NUM_21)

#define I2S_SCK_IO (GPIO_NUM_25)
#define I2S_BCK_IO (GPIO_NUM_26)
#define I2S_LRCK_IO (GPIO_NUM_27)
#define I2S_DIN_IO (GPIO_NUM_5)

static const char* TAG = "MAIN";

esp_err_t init_i2c(void) {
  i2c_port_t port = I2C_NUM_0;
  i2c_config_t config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_SDA_IO,
    .scl_io_num = I2C_SCL_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {
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

extern "C" void app_main(void)
{
  ESP_LOGI(TAG, "Initialising peripherals");

  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LOWMED));
  init_i2c();
  init_spi();

  ESP_LOGI(TAG, "Setting default GPIO state");
  gay_ipod::GpioExpander expander;

  // for debugging usb ic
  //expander.set_sd_mux(gay_ipod::GpioExpander::USB);

  ESP_ERROR_CHECK(expander.Write());

  ESP_LOGI(TAG, "Init ADC");
  ESP_ERROR_CHECK(gay_ipod::init_adc());

  ESP_LOGI(TAG, "Everything looks good! Waiting a mo for debugger.");
  vTaskDelay(pdMS_TO_TICKS(2500));

  ESP_LOGI(TAG, "Trying to init SD card");
  gay_ipod::SdStorage storage(&expander);

  ESP_ERROR_CHECK(storage.Acquire());

  ESP_LOGI(TAG, "Looks okay? Trying to deinit now.");
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_ERROR_CHECK(storage.Release());

  ESP_LOGI(TAG, "Hooray!");
  vTaskDelay(pdMS_TO_TICKS(1000));
}
