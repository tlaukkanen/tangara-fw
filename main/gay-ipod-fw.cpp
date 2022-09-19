#include <stdio.h>

#include "esp_log.h"
#include "driver/i2c.h"

#define I2C_SDA_IO (0)
#define I2C_SCL_IO (4)
// TODO: check if fast mode i2c (400000) will work.
#define I2C_CLOCK_HZ (100000)

esp_err_t init_i2c(void) {
  int i2c_port = 0;
  i2c_config_t config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_SDA_IO,
    .scl_io_num = I2C_SCL_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {
      .clk_speed = I2C_CLOCK_HZ,
    },
    .clk_flags = 0,
  };
  // TODO: INT line?

  ESP_ERROR_CHECK(i2c_param_config(i2c_port, &config));
  return i2c_driver_install(i2c_port, config.mode, 0, 0, 0);
}

extern "C" void app_main(void)
{
  init_i2c();
}
