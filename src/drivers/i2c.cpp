/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "i2c.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "esp_err.h"

namespace drivers {

static const i2c_port_t kI2CPort = I2C_NUM_0;
static const gpio_num_t kI2CSdaPin = GPIO_NUM_4;
static const gpio_num_t kI2CSclPin = GPIO_NUM_2;
static const uint32_t kI2CClkSpeed = 400'000;

static constexpr int kCmdLinkSize = I2C_LINK_RECOMMENDED_SIZE(12);

esp_err_t init_i2c(void) {
  i2c_config_t config = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = kI2CSdaPin,
      .scl_io_num = kI2CSclPin,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master =
          {
              .clk_speed = kI2CClkSpeed,
          },
      // No requirements for the clock.
      .clk_flags = 0,
  };

  if (esp_err_t err = i2c_param_config(kI2CPort, &config)) {
    return err;
  }
  if (esp_err_t err = i2c_driver_install(kI2CPort, config.mode, 0, 0, 0)) {
    return err;
  }
  if (esp_err_t err = i2c_set_timeout(kI2CPort, 400000)) {
    return err;
  }

  // TODO: INT line

  return ESP_OK;
}

esp_err_t deinit_i2c(void) {
  return i2c_driver_delete(kI2CPort);
}

I2CTransaction::I2CTransaction() {
  // Use a fixed size buffer to avoid many many tiny allocations.
  buffer_ = (uint8_t*)calloc(sizeof(uint8_t), kCmdLinkSize);
  handle_ = i2c_cmd_link_create_static(buffer_, kCmdLinkSize);
  assert(handle_ != NULL && "failed to create command link");
}

I2CTransaction::~I2CTransaction() {
  free(buffer_);
}

esp_err_t I2CTransaction::Execute(uint8_t port) {
  return i2c_master_cmd_begin(port, handle_, kI2CTimeout);
}

I2CTransaction& I2CTransaction::start() {
  ESP_ERROR_CHECK(i2c_master_start(handle_));
  return *this;
}

I2CTransaction& I2CTransaction::stop() {
  ESP_ERROR_CHECK(i2c_master_stop(handle_));
  return *this;
}

I2CTransaction& I2CTransaction::write_addr(uint8_t addr, uint8_t op) {
  write_ack(addr << 1 | op);
  return *this;
}

I2CTransaction& I2CTransaction::write_ack(uint8_t data) {
  ESP_ERROR_CHECK(i2c_master_write_byte(handle_, data, true));
  return *this;
}

I2CTransaction& I2CTransaction::read(uint8_t* dest, i2c_ack_type_t ack) {
  ESP_ERROR_CHECK(i2c_master_read_byte(handle_, dest, ack));
  return *this;
}

}  // namespace drivers
