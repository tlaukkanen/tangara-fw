/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "wm8523.hpp"

#include <cstdint>

#include "esp_err.h"

#include "hal/i2c_types.h"
#include "i2c.hpp"

namespace drivers {
namespace wm8523 {

static const uint8_t kAddress = 0b0011010;

auto ReadRegister(Register reg) -> std::optional<uint16_t> {
  uint8_t msb, lsb;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(static_cast<uint8_t>(reg))
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&msb, I2C_MASTER_ACK)
      .read(&lsb, I2C_MASTER_LAST_NACK)
      .stop();
  if (transaction.Execute() != ESP_OK) {
    return {};
  }
  return (msb << 8) & lsb;
}

auto WriteRegister(Register reg, uint16_t data) -> bool {
  return WriteRegister(reg, (data >> 8) & 0xFF, data & 0xFF);
}

auto WriteRegister(Register reg, uint8_t msb, uint8_t lsb) -> bool {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(static_cast<uint8_t>(reg), msb, lsb)
      .stop();
  return transaction.Execute() == ESP_OK;
}

}  // namespace wm8523
}  // namespace drivers
