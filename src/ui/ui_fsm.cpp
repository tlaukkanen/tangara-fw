/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui_fsm.hpp"
#include <memory>
#include "core/lv_obj.h"
#include "display.hpp"
#include "lvgl_task.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "screen_menu.hpp"
#include "screen_splash.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"

namespace ui {

drivers::GpioExpander* UiState::sGpioExpander;
std::shared_ptr<drivers::TouchWheel> UiState::sTouchWheel;
std::shared_ptr<drivers::RelativeWheel> UiState::sRelativeWheel;
std::shared_ptr<drivers::Display> UiState::sDisplay;

std::shared_ptr<Screen> UiState::sCurrentScreen;

auto UiState::Init(drivers::GpioExpander* gpio_expander) -> bool {
  sGpioExpander = gpio_expander;

  lv_init();
  sDisplay.reset(
      drivers::Display::Create(gpio_expander, drivers::displays::kST7735R));
  if (sDisplay == nullptr) {
    return false;
  }

  sTouchWheel.reset(drivers::TouchWheel::Create());
  if (sTouchWheel != nullptr) {
    sRelativeWheel.reset(new drivers::RelativeWheel(sTouchWheel.get()));
  }

  sCurrentScreen.reset(new screens::Splash());

  // Start the UI task even if init ultimately failed, so that we can show some
  // kind of error screen to the user.
  StartLvgl(sRelativeWheel, sDisplay);

  if (sTouchWheel == nullptr) {
    return false;
  }
  return true;
}

namespace states {

void Splash::exit() {
  if (sDisplay != nullptr) {
    sDisplay->SetDisplayOn(true);
  }
}

void Splash::react(const system_fsm::BootComplete& ev) {
  transit<Interactive>();
}

void Interactive::entry() {
  sCurrentScreen.reset(new screens::Menu());
}

void Interactive::react(const system_fsm::KeyLockChanged& ev) {
  sDisplay->SetDisplayOn(ev.falling);
}

}  // namespace states
}  // namespace ui

FSM_INITIAL_STATE(ui::UiState, ui::states::Splash)
