/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "app_console.hpp"
#include "battery.hpp"
#include "database.hpp"
#include "display.hpp"
#include "gpio_expander.hpp"
#include "relative_wheel.hpp"
#include "samd.hpp"
#include "storage.hpp"
#include "tinyfsm.hpp"
#include "touchwheel.hpp"

#include "system_events.hpp"

namespace system_fsm {

/*
 * State machine for the overall system state. Responsible for managing
 * peripherals, and bringing the rest of the system up and down.
 */
class SystemState : public tinyfsm::Fsm<SystemState> {
 public:
  virtual ~SystemState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  void react(const FatalError&);

  virtual void react(const DisplayReady&) {}
  virtual void react(const BootComplete&) {}
  virtual void react(const StorageUnmountRequested&) {}
  virtual void react(const internal::ReadyToUnmount&) {}
  virtual void react(const StorageMounted&) {}
  virtual void react(const StorageError&) {}

 protected:
  static std::shared_ptr<drivers::GpioExpander> sGpioExpander;
  static std::shared_ptr<drivers::Samd> sSamd;

  static std::shared_ptr<drivers::TouchWheel> sTouch;
  static std::shared_ptr<drivers::RelativeWheel> sRelativeTouch;
  static std::shared_ptr<drivers::Battery> sBattery;
  static std::shared_ptr<drivers::SdStorage> sStorage;
  static std::shared_ptr<drivers::Display> sDisplay;
  static std::shared_ptr<database::Database> sDatabase;
};

namespace states {

/*
 * Initial state. Initialises peripherals, starts up lvgl, checks everything
 * looks good.
 */
class Booting : public SystemState {
 private:
  static console::AppConsole* sAppConsole;

 public:
  void entry() override;
  void exit() override;

  void react(const BootComplete&) override;
  using SystemState::react;
};

/*
 * Most common state. Everything is going full bore!
 */
class Running : public SystemState {
 public:
  void entry() override;
  void exit() override;

  void react(const StorageUnmountRequested&) override;
  void react(const internal::ReadyToUnmount&) override;
  void react(const StorageError&) override;
  using SystemState::react;
};

class Unmounted : public SystemState {};

/*
 * Something unrecoverably bad went wrong. Shows an error (if possible), awaits
 * reboot.
 */
class Error : public SystemState {};

}  // namespace states

}  // namespace system_fsm
