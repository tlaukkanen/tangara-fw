/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/samd.hpp"
#include <stdint.h>

#include <cstdint>
#include <optional>
#include <string>

#include "drivers/nvs.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "drivers/i2c.hpp"

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

auto Samd::chargeStatusToString(ChargeStatus status) -> std::string {
  switch (status) {
    case ChargeStatus::kNoBattery:
      return "no_battery";
    case ChargeStatus::kBatteryCritical:
      return "critical";
    case ChargeStatus::kDischarging:
      return "discharging";
    case ChargeStatus::kChargingRegular:
      return "charge_regular";
    case ChargeStatus::kChargingFast:
      return "charge_fast";
    case ChargeStatus::kFullCharge:
      return "full_charge";
    case ChargeStatus::kFault:
      return "fault";
    case ChargeStatus::kUnknown:
    default:
      return "unknown";
  }
}

Samd::Samd(NvsStorage& nvs) : nvs_(nvs) {
  gpio_set_direction(kIntPin, GPIO_MODE_INPUT);

  // Being able to interface with the SAMD properly is critical. To ensure we
  // will be able to, we begin by checking the I2C protocol version is
  // compatible, and throw if it's not.
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kSamdFirmwareVersion)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&version_, I2C_MASTER_NACK)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(1));
  ESP_LOGI(kTag, "samd firmware rev: %u", version_);

  UpdateChargeStatus();
  UpdateUsbStatus();
  SetFastChargeEnabled(nvs.FastCharge());
}
Samd::~Samd() {}

auto Samd::Version() -> std::string {
  return std::to_string(version_);
}

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

  // Lower two bits are the usb power status, next three are the BMS status.
  // See 'gpio.c' in the SAMD21 firmware for how these bits get packed.
  uint8_t charge_state = (raw_res & 0b11100) >> 2;
  uint8_t usb_state = raw_res & 0b11;
  switch (charge_state) {
    case 0b000:
      charge_status_ = ChargeStatus::kNoBattery;
      break;
    case 0b001:
      // BMS says we're charging; work out how fast we're charging.
      if (usb_state >= 0b10 && nvs_.FastCharge()) {
        charge_status_ = ChargeStatus::kChargingFast;
      } else {
        charge_status_ = ChargeStatus::kChargingRegular;
      }
      break;
    case 0b010:
      charge_status_ = ChargeStatus::kFullCharge;
      break;
    case 0b011:
      charge_status_ = ChargeStatus::kFault;
      break;
    case 0b100:
      charge_status_ = ChargeStatus::kBatteryCritical;
      break;
    case 0b101:
      charge_status_ = ChargeStatus::kDischarging;
      break;
    case 0b110:
    case 0b111:
      charge_status_ = ChargeStatus::kUnknown;
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
      (raw_res & 0b10) ? UsbStatus::kAttachedBusy : UsbStatus::kAttachedIdle;
}

auto Samd::ResetToFlashSamd() -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbControl, 0b100)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::SetFastChargeEnabled(bool en) -> void {
  // Always update NVS, so that the setting is right after the SAMD firmware is
  // updated.
  nvs_.FastCharge(en);

  if (version_ < 4) {
    return;
  }
  ESP_LOGI(kTag, "set fast charge %u", en);

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kPowerControl, (en << 1))
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

auto Samd::UsbMassStorage(bool en) -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbControl, en)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::UsbMassStorage() -> bool {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(Registers::kUsbControl)
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&raw_res, I2C_MASTER_NACK)
      .stop();
  esp_err_t res = transaction.Execute(1);
  if (res != ESP_OK) {
    return false;
  }
  return raw_res & 1;
}

}  // namespace drivers
