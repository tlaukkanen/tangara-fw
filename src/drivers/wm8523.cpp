/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "drivers/wm8523.hpp"

#include <cstdint>

#include "esp_err.h"

#include "drivers/i2c.hpp"
#include "hal/i2c_types.h"

namespace drivers {
namespace wm8523 {

const uint16_t kAbsoluteMaxVolume = 0x1ff;
const uint16_t kAbsoluteMinVolume = 0b0;

// This is 3dB below what the DAC considers to be '0dB', and 9.5dB above line
// level reference.
const uint16_t kMaxVolumeBeforeClipping = 0x184;

// This is 12.5 dB below what the DAC considers to be '0dB'.
const uint16_t kLineLevelReferenceVolume = 0x15E;

// Default to -24 dB, which I will claim is 'arbitrarily chosen to be safe but
// audible', but is in fact just a nice value for my headphones in particular.
const uint16_t kDefaultVolume = kLineLevelReferenceVolume - 96;

// Default to +6dB == 2Vrms == 'CD Player'
const uint16_t kDefaultMaxVolume = kLineLevelReferenceVolume + 12;

const uint16_t kZeroDbVolume = 0x190;

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
  return (msb << 8) | lsb;
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
