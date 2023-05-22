#pragma once

#include <memory>

#include "database.hpp"
#include "display.hpp"
#include "storage.hpp"
#include "tinyfsm.hpp"
#include "touchwheel.hpp"

#include "system_events.hpp"

namespace ui {

class UiState : public tinyfsm::Fsm<UiState> {
 public:
  static auto Init(drivers::GpioExpander* gpio_expander,
                   std::weak_ptr<drivers::TouchWheel> touchwheel,
                   std::weak_ptr<drivers::Display> display,
                   std::weak_ptr<database::Database> database) -> void;

  virtual ~UiState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  virtual void react(const system_fsm::DisplayReady&) {}
  virtual void react(const system_fsm::BootComplete&) {}

 protected:
  static drivers::GpioExpander* sGpioExpander;
  static std::weak_ptr<drivers::TouchWheel> sTouchWheel;
  static std::weak_ptr<drivers::Display> sDisplay;
  static std::weak_ptr<database::Database> sDatabase;
};

namespace states {

class PreBoot : public UiState {
 public:
  void react(const system_fsm::DisplayReady&) override;
  using UiState::react;
};

class Splash : public UiState {
 public:
  void react(const system_fsm::BootComplete&) override;
  using UiState::react;
};

class Interactive : public UiState {};

class FatalError : public UiState {};

}  // namespace states

}  // namespace ui
