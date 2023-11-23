/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "samd.hpp"

#include <cstdint>
#include <optional>

#include "esp_err.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
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
[[maybe_unused]] static const char kTag[] = "SAMD";

namespace drivers {

static constexpr gpio_num_t kIntPin = GPIO_NUM_35;

Samd::Samd() {
  gpio_set_direction(kIntPin, GPIO_MODE_INPUT);

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
  ESP_ERROR_CHECK(transaction.Execute(1));
  ESP_LOGI(kTag, "samd firmware rev: %u", raw_res);

  UpdateChargeStatus();
  UpdateUsbStatus();
}
Samd::~Samd() {}

auto Samd::GetChargeStatus() -> std::optional<ChargeStatus> {
  return charge_status_;
}

auto Samd::UpdateChargeStatus() -> void {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kChargeStatus)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&raw_res, I2C_MASTER_NACK)
      .stop();
  esp_err_t res = transaction.Execute(1);
  if (res != ESP_OK) {
    return;
  }

  uint8_t usb_state = raw_res & 0b11;
  uint8_t charge_state = (raw_res >> 2) & 0b111;
  switch (charge_state) {
    case 0b000:
    case 0b011:
      charge_status_ = ChargeStatus::kNoBattery;
      break;
    case 0b001:
      charge_status_ = usb_state == 1 ? ChargeStatus::kChargingRegular
                                      : ChargeStatus::kChargingFast;
      break;
    case 0b010:
      charge_status_ = ChargeStatus::kFullCharge;
      break;
    case 0b100:
      charge_status_ = ChargeStatus::kBatteryCritical;
      break;
    case 0b101:
      charge_status_ = ChargeStatus::kDischarging;
      break;
    default:
      charge_status_ = {};
      break;
  }
}

auto Samd::GetUsbStatus() -> UsbStatus {
  return usb_status_;
}

auto Samd::UpdateUsbStatus() -> void {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbStatus)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&raw_res, I2C_MASTER_NACK)
      .stop();
  esp_err_t res = transaction.Execute(1);
  if (res != ESP_OK) {
    return;
  }

  if (!(raw_res & 0b1)) {
    usb_status_ = UsbStatus::kDetached;
  }
  usb_status_ =
      (raw_res & 0b10) ? UsbStatus::kAttachedMounted : UsbStatus::kAttachedIdle;
}

auto Samd::ResetToFlashSamd() -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbControl, 0b100)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::PowerDown() -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kPowerControl, 0b1)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

}  // namespace drivers
