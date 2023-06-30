/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_fsm.hpp"
#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "relative_wheel.hpp"
#include "system_events.hpp"

namespace system_fsm {

std::shared_ptr<drivers::GpioExpander> SystemState::sGpioExpander;
std::shared_ptr<drivers::Samd> SystemState::sSamd;

std::shared_ptr<drivers::TouchWheel> SystemState::sTouch;
std::shared_ptr<drivers::RelativeWheel> SystemState::sRelativeTouch;
std::shared_ptr<drivers::Battery> SystemState::sBattery;
std::shared_ptr<drivers::SdStorage> SystemState::sStorage;
std::shared_ptr<drivers::Display> SystemState::sDisplay;
std::shared_ptr<database::Database> SystemState::sDatabase;

console::AppConsole* SystemState::sAppConsole;

void SystemState::react(const FatalError& err) {
  if (!is_in_state<states::Error>()) {
    transit<states::Error>();
  }
}

void SystemState::react(const internal::GpioInterrupt& ev) {
  ESP_LOGI("sys", "gpios changed");
  bool prev_key_up = sGpioExpander->get_input(drivers::GpioExpander::KEY_UP);
  bool prev_key_down =
      sGpioExpander->get_input(drivers::GpioExpander::KEY_DOWN);
  bool prev_key_lock =
      sGpioExpander->get_input(drivers::GpioExpander::KEY_LOCK);
  bool prev_has_headphones =
      sGpioExpander->get_input(drivers::GpioExpander::PHONE_DETECT);

  sGpioExpander->Read();

  bool key_up = sGpioExpander->get_input(drivers::GpioExpander::KEY_UP);
  bool key_down = sGpioExpander->get_input(drivers::GpioExpander::KEY_DOWN);
  bool key_lock = sGpioExpander->get_input(drivers::GpioExpander::KEY_LOCK);
  bool has_headphones =
      sGpioExpander->get_input(drivers::GpioExpander::PHONE_DETECT);

  if (key_up != prev_key_up) {
    events::Dispatch<KeyUpChanged, audio::AudioState, ui::UiState>(
        {.falling = prev_key_up});
  }
  if (key_down != prev_key_down) {
    events::Dispatch<KeyDownChanged, audio::AudioState, ui::UiState>(
        {.falling = prev_key_down});
  }
  if (key_lock != prev_key_lock) {
    events::Dispatch<KeyLockChanged, SystemState, ui::UiState>(
        {.falling = prev_key_lock});
  }
  if (has_headphones != prev_has_headphones) {
    events::Dispatch<HasPhonesChanged, audio::AudioState>(
        {.falling = prev_has_headphones});
  }
}

}  // namespace system_fsm

FSM_INITIAL_STATE(system_fsm::SystemState, system_fsm::states::Booting)
