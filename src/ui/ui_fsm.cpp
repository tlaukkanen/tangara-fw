/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui_fsm.hpp"
#include "display.hpp"
#include "lvgl_task.hpp"
#include "screen.hpp"
#include "screen_menu.hpp"
#include "screen_splash.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"

namespace ui {

drivers::GpioExpander* UiState::sGpioExpander;
std::weak_ptr<drivers::TouchWheel> UiState::sTouchWheel;
std::weak_ptr<drivers::Display> UiState::sDisplay;
std::weak_ptr<database::Database> UiState::sDatabase;

std::shared_ptr<Screen> UiState::sCurrentScreen;

auto UiState::Init(drivers::GpioExpander* gpio_expander,
                   std::weak_ptr<drivers::TouchWheel> touchwheel,
                   std::weak_ptr<drivers::Display> display,
                   std::weak_ptr<database::Database> database) -> void {
  sGpioExpander = gpio_expander;
  sTouchWheel = touchwheel;
  sDisplay = display;
  sDatabase = database;
}

namespace states {

void PreBoot::react(const system_fsm::DisplayReady& ev) {
  transit<Splash>([&]() { StartLvgl(sTouchWheel, sDisplay); });
}

void Splash::entry() {
  sCurrentScreen.reset(new screens::Splash());
}

void Splash::react(const system_fsm::BootComplete& ev) {
  transit<Interactive>();
}

void Interactive::entry() {
  // sCurrentScreen.reset(new screens::Menu());
}

}  // namespace states
}  // namespace ui

FSM_INITIAL_STATE(ui::UiState, ui::states::PreBoot)
