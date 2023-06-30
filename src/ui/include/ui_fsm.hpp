/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "relative_wheel.hpp"
#include "tinyfsm.hpp"

#include "display.hpp"
#include "screen.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"

namespace ui {

class UiState : public tinyfsm::Fsm<UiState> {
 public:
  static auto Init(drivers::GpioExpander* gpio_expander) -> bool;

  virtual ~UiState() {}

  static auto current_screen() -> std::shared_ptr<Screen> {
    return sCurrentScreen;
  }

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  virtual void react(const system_fsm::KeyLockChanged&){};

  virtual void react(const system_fsm::DisplayReady&) {}
  virtual void react(const system_fsm::BootComplete&) {}

 protected:
  static drivers::GpioExpander* sGpioExpander;
  static std::shared_ptr<drivers::TouchWheel> sTouchWheel;
  static std::shared_ptr<drivers::RelativeWheel> sRelativeWheel;
  static std::shared_ptr<drivers::Display> sDisplay;

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

  void react(const system_fsm::KeyLockChanged&) override;
};

class FatalError : public UiState {};

}  // namespace states

}  // namespace ui
