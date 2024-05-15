/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <optional>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace drivers {

class Samd {
 public:
  static auto Create() -> Samd* { return new Samd(); }

  Samd();
  ~Samd();

  auto Version() -> std::string;

  enum class ChargeStatus {
    // There is no battery plugged into the device.
    kNoBattery,
    // The battery is discharging, and the current voltage level is very low.
    kBatteryCritical,
    // The battery is discharging.
    kDischarging,
    // The battery is charging over a low-current USB connection
    kChargingRegular,
    // The battery is charging over a high-current USB connection
    kChargingFast,
    // The battery is full charged, and we are still plugged in.
    kFullCharge,
  };

  auto GetChargeStatus() -> std::optional<ChargeStatus>;
  auto UpdateChargeStatus() -> void;

  enum class UsbStatus {
    // There is no compatible usb host attached.
    kDetached,
    // There is a compatible usb host attached, but USB MSC is not currently
    // in use by the SAMD.
    kAttachedIdle,
    // The SAMD is currently writing to the SD card via USB MSC.
    kAttachedBusy,
  };

  auto GetUsbStatus() -> UsbStatus;
  auto UpdateUsbStatus() -> void;

  auto ResetToFlashSamd() -> void;
  auto SetFastChargeEnabled(bool) -> void;
  auto PowerDown() -> void;

  auto UsbMassStorage(bool en) -> void;
  auto UsbMassStorage() -> bool;

  // Not copyable or movable. There should usually only ever be once instance
  // of this class, and that instance will likely have a static lifetime.
  Samd(const Samd&) = delete;
  Samd& operator=(const Samd&) = delete;

 private:
  uint8_t version_;
  std::optional<ChargeStatus> charge_status_;
  UsbStatus usb_status_;
};

}  // namespace drivers
