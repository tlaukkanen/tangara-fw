#include "touchwheel.hpp"
#include <stdint.h>

#include <cstdint>

#include "assert.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "i2c.hpp"

namespace drivers {

static const char* kTag = "TOUCHWHEEL";
static const uint8_t kTouchWheelAddress = 0x1C;

TouchWheel::TouchWheel() {
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_INPUT);
  gpio_set_pull_mode(GPIO_NUM_25, GPIO_PULLUP_ONLY);

  WriteRegister(Register::RESET, 1);
  // TODO(daniel): do we need this? how long does reset take?
  vTaskDelay(pdMS_TO_TICKS(1));
  WriteRegister(Register::SLIDER_OPTIONS, 0b11000000);
  WriteRegister(Register::CALIBRATE, 1);
}

TouchWheel::~TouchWheel() {}

void TouchWheel::WriteRegister(uint8_t reg, uint8_t val) {
  // uint8_t maskedReg = reg | kWriteMask;
  uint8_t maskedReg = reg;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg, val)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute());
}

uint8_t TouchWheel::ReadRegister(uint8_t reg) {
  uint8_t res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_WRITE)
      .write_ack(reg)
      .start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_READ)
      .read(&res, I2C_MASTER_NACK)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute());
  return res;
}

void TouchWheel::Update() {
  // Read data from device into member struct
  bool has_data = !gpio_get_level(GPIO_NUM_25);
  if (!has_data) {
    return;
  }
  uint8_t status = ReadRegister(Register::DETECTION_STATUS);
  if (status & 0b10000000) {
    // Still calibrating.
    return;
  }
  if (status & 0b10) {
    // Slider detect.
    data_.wheel_position = ReadRegister(Register::SLIDER_POSITION);
  }
  if (status & 0b1) {
    // Key detect.
    // TODO(daniel): implement me
  }
}

TouchWheelData TouchWheel::GetTouchWheelData() const {
  return data_;
}

}  // namespace drivers
