/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

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
static const gpio_num_t kIntPin = GPIO_NUM_25;

TouchWheel::TouchWheel() {
  gpio_config_t int_config{
      .pin_bit_mask = 1ULL << kIntPin,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&int_config);

  // Configure keys 0, 1, and 2 as a wheel.
  WriteRegister(SLIDER_OPTIONS, 0b11000000);

  // Configure adjacent key suppression.
  // Wheel keys. Set to channel 1.
  WriteRegister(Register::KEY_CONTROL_BASE + 0, 0b100);
  WriteRegister(Register::KEY_CONTROL_BASE + 1, 0b100);
  WriteRegister(Register::KEY_CONTROL_BASE + 2, 0b100);
  // Centre button. Set to channel 1.
  WriteRegister(Register::KEY_CONTROL_BASE + 3, 0b100);
  // Touch guard. Set as a guard, in channel 1.
  WriteRegister(Register::KEY_CONTROL_BASE + 4, 0b10100);

  // Unused extra keys. All disabled.
  for (int i = 5; i < 12; i++) {
    WriteRegister(Register::KEY_CONTROL_BASE + i, 1);
  }
}

TouchWheel::~TouchWheel() {}

void TouchWheel::WriteRegister(uint8_t reg, uint8_t val) {
  // Addresses <= 5 are not writeable. Make sure we don't try.
  assert(reg > 5);
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_WRITE)
      .write_ack(reg, val)
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
  bool has_data = !gpio_get_level(kIntPin);
  if (!has_data) {
    return;
  }
  uint8_t status = ReadRegister(Register::DETECTION_STATUS);
  if (status & 0b10000000) {
    // Still calibrating.
    ESP_LOGW(kTag, "awaiting calibration");
    return;
  }
  if (status & 0b01000000) {
    // Probably okay, but we should keep an eye on this for development.
    ESP_LOGW(kTag, "touchwheel acquisition >16ms");
  }
  if (status & 0b10) {
    // Slider detect.
    uint8_t pos = ReadRegister(Register::SLIDER_POSITION);
    data_.wheel_position = pos;
  }
  if (status & 0b1) {
    // Key detect. Note that the touchwheel keys also trigger this.
    // TODO(daniel): Do something with this.
    // bool centre_key = ReadRegister(Register::KEY_STATUS_A) & 0b1000;
  }
}

TouchWheelData TouchWheel::GetTouchWheelData() const {
  return data_;
}

}  // namespace drivers
