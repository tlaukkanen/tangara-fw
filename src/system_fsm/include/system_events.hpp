/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "database.hpp"
#include "tinyfsm.hpp"

namespace system_fsm {

struct DisplayReady : tinyfsm::Event {};

/*
 * Sent by SysState when the system has finished with its boot and self-test,
 * and is now ready to run normally.
 */
struct BootComplete : tinyfsm::Event {};

/*
 * May be sent by any component to indicate that the system has experienced an
 * unrecoverable error. This should be used sparingly, as it essentially brings
 * down the device.
 */
struct FatalError : tinyfsm::Event {};

/*
 * Sent before unmounting the system storage. Storage will not be unmounted
 * until each reaction to this even has returned. FSMs should immediately cease
 * their usage of storage.
 *
 * May be emitted either by UiState in response to user action, or by SysState
 * as a part of either entering low-power standby or powering off.
 */
struct StorageUnmountRequested : tinyfsm::Event {};

/*
 * Sent by SysState when the system storage has been successfully mounted.
 */
struct StorageMounted : tinyfsm::Event {
  std::weak_ptr<database::Database> db;
};

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

namespace internal {

/*
 * Sent when the actual unmount operation should be performed. Always dispatched
 * by SysState in response to StoragePrepareToUnmount.
 */
struct ReadyToUnmount : tinyfsm::Event {};

struct GpioInterrupt : tinyfsm::Event {};
struct SamdInterrupt : tinyfsm::Event {};

}  // namespace internal

}  // namespace system_fsm
