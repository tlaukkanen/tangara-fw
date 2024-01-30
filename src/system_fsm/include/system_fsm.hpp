/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "app_console.hpp"
#include "audio_events.hpp"
#include "battery.hpp"
#include "bluetooth.hpp"
#include "database.hpp"
#include "db_events.hpp"
#include "display.hpp"
#include "gpios.hpp"
#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "samd.hpp"
#include "service_locator.hpp"
#include "storage.hpp"
#include "tag_parser.hpp"
#include "tinyfsm.hpp"
#include "touchwheel.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "system_events.hpp"
#include "track_queue.hpp"

namespace system_fsm {

void check_interrupts_cb(TimerHandle_t timer);

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
  void react(const internal::GpioInterrupt&);
  void react(const internal::SamdInterrupt&);

  void react(const HapticTrigger&);

  virtual void react(const DisplayReady&) {}
  virtual void react(const BootComplete&) {}
  virtual void react(const StorageMounted&) {}
  virtual void react(const StorageError&) {}
  virtual void react(const KeyLockChanged&) {}
  virtual void react(const SdDetectChanged&) {}
  virtual void react(const database::event::UpdateFinished&) {}
  virtual void react(const audio::PlaybackFinished&) {}
  virtual void react(const internal::IdleTimeout&) {}

 protected:
  auto IdleCondition() -> bool;

  static std::shared_ptr<ServiceLocator> sServices;
  static std::unique_ptr<drivers::SdStorage> sStorage;

  static console::AppConsole* sAppConsole;
};

namespace states {

/*
 * Initial state. Initialises peripherals, starts up lvgl, checks everything
 * looks good.
 */
class Booting : public SystemState {
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

  void react(const KeyLockChanged&) override;
  void react(const SdDetectChanged&) override;
  void react(const audio::PlaybackFinished&) override;
  void react(const database::event::UpdateFinished&) override;

  using SystemState::react;

 private:
  auto mountStorage() -> bool;
  auto unmountStorage() -> void;

  bool storage_mounted_;
};

/**
 * State for when the screen is off, controls locked, and music paused. Prelude
 * to shutting off power completely.
 */
class Idle : public SystemState {
 public:
  void entry() override;
  void exit() override;

  void react(const KeyLockChanged&) override;
  void react(const internal::IdleTimeout&) override;

  using SystemState::react;

 private:
  TimerHandle_t sIdleTimeout;
};

/*
 * Something unrecoverably bad went wrong. Shows an error (if possible), awaits
 * reboot.
 */
class Error : public SystemState {};

}  // namespace states

}  // namespace system_fsm
