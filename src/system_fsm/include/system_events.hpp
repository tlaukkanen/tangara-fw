/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "database.hpp"
#include "service_locator.hpp"
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

struct StorageError : tinyfsm::Event {};

struct KeyUpChanged : tinyfsm::Event {
  bool falling;
};
struct KeyDownChanged : tinyfsm::Event {
  bool falling;
};
struct KeyLockChanged : tinyfsm::Event {
  bool falling;
};
struct HasPhonesChanged : tinyfsm::Event {
  bool falling;
};

struct ChargingStatusChanged : tinyfsm::Event {};
struct BatteryStateChanged : tinyfsm::Event {};

namespace internal {

struct GpioInterrupt : tinyfsm::Event {};
struct SamdInterrupt : tinyfsm::Event {};

struct IdleTimeout : tinyfsm::Event {};

}  // namespace internal

}  // namespace system_fsm
