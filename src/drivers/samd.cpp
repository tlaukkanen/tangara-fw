/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "samd.hpp"
#include <stdint.h>
#include <optional>

#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"
#include "i2c.hpp"

enum Registers : uint8_t {
  kSamdFirmwareVersion = 0,
  kChargeStatus = 1,
  kUsbStatus = 2,
  kPowerControl = 3,
  kUsbControl = 4,
};

static const uint8_t kAddress = 0x45;
static const char kTag[] = "SAMD";

namespace drivers {

Samd::Samd() {
  // Being able to interface with the SAMD properly is critical. To ensure we
  // will be able to, we begin by checking the I2C protocol version is
  // compatible, and throw if it's not.
  uint8_t raw_res = 0;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kSamdFirmwareVersion)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&raw_res, I2C_MASTER_NACK)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute());
  ESP_LOGI(kTag, "samd firmware rev: %u", raw_res);
}
Samd::~Samd() {}

auto Samd::ReadChargeStatus() -> std::optional<ChargeStatus> {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kChargeStatus)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&raw_res, I2C_MASTER_NACK)
      .stop();
  ESP_LOGI(kTag, "checking charge status");
  ESP_ERROR_CHECK(transaction.Execute());
  ESP_LOGI(kTag, "raw charge status: %x", raw_res);

  uint8_t usb_state = raw_res & 0b11;
  uint8_t charge_state = (raw_res >> 2) & 0b111;
  switch (charge_state) {
    case 0:
      return ChargeStatus::kNoBattery;
    case 1:
      return usb_state == 1 ? ChargeStatus::kChargingRegular
                            : ChargeStatus::kChargingFast;
    case 2:
      return ChargeStatus::kFullCharge;
    case 4:
      return ChargeStatus::kBatteryCritical;
    case 5:
      return ChargeStatus::kDischarging;
    case 3:
      // Fall-through.
    default:
      return {};
  }
}

auto Samd::ReadUsbStatus() -> UsbStatus {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbStatus)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&raw_res, I2C_MASTER_NACK)
      .stop();
  ESP_LOGI(kTag, "checking usb status");
  ESP_ERROR_CHECK(transaction.Execute());
  ESP_LOGI(kTag, "raw usb status: %x", raw_res);

  if (!(raw_res & 0b1)) {
    return UsbStatus::kDetached;
  }
  return (raw_res & 0b10) ? UsbStatus::kAttachedMounted
                          : UsbStatus::kAttachedIdle;
}

auto Samd::ResetToFlashSamd() -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbControl, 0b100)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute());
}

}  // namespace drivers
