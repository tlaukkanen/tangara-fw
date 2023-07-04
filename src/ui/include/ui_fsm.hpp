/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <stack>

#include "relative_wheel.hpp"
#include "tinyfsm.hpp"

#include "display.hpp"
#include "screen.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "ui_events.hpp"

namespace ui {

class UiState : public tinyfsm::Fsm<UiState> {
 public:
  static auto Init(drivers::IGpios* gpio_expander) -> bool;

  virtual ~UiState() {}

  static auto current_screen() -> std::shared_ptr<Screen> {
    return sCurrentScreen;
  }

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  virtual void react(const system_fsm::KeyLockChanged&){};

  virtual void react(const internal::RecordSelected&){};
  virtual void react(const internal::IndexSelected&){};

  virtual void react(const system_fsm::DisplayReady&) {}
  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const system_fsm::StorageMounted&) {}

 protected:
  void PushScreen(std::shared_ptr<Screen>);

  static drivers::IGpios* sIGpios;
  static std::shared_ptr<drivers::TouchWheel> sTouchWheel;
  static std::shared_ptr<drivers::RelativeWheel> sRelativeWheel;
  static std::shared_ptr<drivers::Display> sDisplay;
  static std::weak_ptr<database::Database> sDb;

  static std::stack<std::shared_ptr<Screen>> sScreens;
  static std::shared_ptr<Screen> sCurrentScreen;
};

namespace states {

class Splash : public UiState {
 public:
  void exit() override;
  void react(const system_fsm::BootComplete&) override;
  using UiState::react;
};

class Interactive : public UiState {
  void entry() override;

  void react(const internal::RecordSelected&) override;
  void react(const internal::IndexSelected&) override;

  void react(const system_fsm::KeyLockChanged&) override;
  void react(const system_fsm::StorageMounted&) override;
};

class FatalError : public UiState {};

}  // namespace states

}  // namespace ui
