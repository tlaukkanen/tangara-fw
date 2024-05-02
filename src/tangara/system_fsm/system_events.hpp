/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "battery/battery.hpp"
#include "bluetooth_types.hpp"
#include "database/database.hpp"
#include "ff.h"
#include "haptics.hpp"
#include "samd.hpp"
#include "system_fsm/service_locator.hpp"
#include "tinyfsm.hpp"

namespace system_fsm {

struct DisplayReady : tinyfsm::Event {};

/*
 * Sent by SysState when the system has finished with its boot and self-test,
 * and is now ready to run normally.
 */
struct BootComplete : tinyfsm::Event {
  std::shared_ptr<ServiceLocator> services;
};

/*
 * May be sent by any component to indicate that the system has experienced an
 * unrecoverable error. This should be used sparingly, as it essentially brings
 * down the device.
 */
struct FatalError : tinyfsm::Event {};

struct OnIdle : tinyfsm::Event {};

/*
 * Sent by SysState when the system storage has been successfully mounted.
 */
struct StorageMounted : tinyfsm::Event {};

struct StorageError : tinyfsm::Event {
  FRESULT error;
};

struct KeyLockChanged : tinyfsm::Event {
  bool locking;
};
struct HasPhonesChanged : tinyfsm::Event {
  bool has_headphones;
};
struct SdDetectChanged : tinyfsm::Event {
  bool has_sd_card;
};

struct SamdUsbMscChanged : tinyfsm::Event {
  bool en;
};
struct SamdUsbStatusChanged : tinyfsm::Event {
  drivers::Samd::UsbStatus new_status;
};

struct BatteryStateChanged : tinyfsm::Event {
  battery::Battery::BatteryState new_state;
};

struct BluetoothEvent : tinyfsm::Event {
  drivers::bluetooth::Event event;
};

struct HapticTrigger : tinyfsm::Event {
  drivers::Haptics::Effect effect;
};

namespace internal {

struct GpioInterrupt : tinyfsm::Event {};
struct SamdInterrupt : tinyfsm::Event {};

struct IdleTimeout : tinyfsm::Event {};
struct UnmountTimeout : tinyfsm::Event {};

}  // namespace internal

}  // namespace system_fsm
