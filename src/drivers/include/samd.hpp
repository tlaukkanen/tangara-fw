#pragma once

#include <optional>

namespace drivers {

class Samd {
 public:
  static auto Create() -> Samd* { return new Samd(); }

  Samd();
  ~Samd();

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

  auto ReadChargeStatus() -> std::optional<ChargeStatus>;

  enum class UsbMscStatus {
    // There is no compatible usb host attached.
    kDetached,
    // There is a compatible usb host attached, but USB MSC is not currently
    // in use by the SAMD.
    kAttachedIdle,
    // The SAMD is currently exposing the SD card via USB MSC.
    kAttachedMounted,
  };

  auto WriteAllowUsbMsc(bool is_allowed) -> void;
  auto ReadUsbMscStatus() -> UsbMscStatus;

  auto ReadFlashingEnabled() -> bool;
  auto WriteFlashingEnabled(bool) -> void;
};

}  // namespace drivers
