/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/samd.hpp"

#include <cstdint>
#include <format>
#include <optional>
#include <string>

#include "esp_err.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "drivers/i2c.hpp"
#include "drivers/nvs.hpp"

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
      .write_ack(registerIdx(RegisterName::kSamdFirmwareMajorVersion))
      .start()
      .write_addr(kAddress, I2C_MASTER_READ)
      .read(&version_major_, I2C_MASTER_ACK)
      .read(&version_minor_, I2C_MASTER_NACK)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(1));

  if (version_major_ < 6) {
    version_minor_ = 0;
  }
  ESP_LOGI(kTag, "samd firmware rev: %u.%u", version_major_, version_minor_);

  UpdateChargeStatus();
  UpdateUsbStatus();
  SetFastChargeEnabled(nvs.FastCharge());
}
Samd::~Samd() {}

auto Samd::Version() -> std::string {
  return std::format("{}.{}", version_major_, version_minor_);
}

auto Samd::GetChargeStatus() -> std::optional<ChargeStatus> {
  return charge_status_;
}

auto Samd::UpdateChargeStatus() -> void {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(registerIdx(RegisterName::kChargeStatus))
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
      .write_ack(registerIdx(RegisterName::kUsbStatus))
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
      .write_ack(registerIdx(RegisterName::kUsbControl), 0b100)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::SetFastChargeEnabled(bool en) -> void {
  // Always update NVS, so that the setting is right after the SAMD firmware is
  // updated.
  nvs_.FastCharge(en);

  if (version_major_ < 4) {
    return;
  }

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(registerIdx(RegisterName::kPowerControl), (en << 1))
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::PowerDown() -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(registerIdx(RegisterName::kPowerControl), 0b1)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::UsbMassStorage(bool en) -> void {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(registerIdx(RegisterName::kUsbControl), en)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute(3));
}

auto Samd::UsbMassStorage() -> bool {
  uint8_t raw_res;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kAddress, I2C_MASTER_WRITE)
      .write_ack(registerIdx(RegisterName::kUsbControl))
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

auto Samd::registerIdx(RegisterName r) -> uint8_t {
  switch (r) {
    case RegisterName::kSamdFirmwareMajorVersion:
      // Register 0 is always the major version.
      return 0;
    case RegisterName::kSamdFirmwareMinorVersion:
      // Firmwares before version 6 had no minor :(
      return version_major_ >= 6 ? 1 : 0;
    case RegisterName::kChargeStatus:
      return version_major_ >= 6 ? 2 : 1;
    case RegisterName::kUsbStatus:
      return version_major_ >= 6 ? 3 : 2;
    case RegisterName::kPowerControl:
      return version_major_ >= 6 ? 4 : 3;
    case RegisterName::kUsbControl:
      return version_major_ >= 6 ? 5 : 4;
  }
  assert(false);  // very very bad!!!
  return 0;
}

}  // namespace drivers
