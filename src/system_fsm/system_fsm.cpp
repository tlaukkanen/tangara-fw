/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_fsm.hpp"
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

void SystemState::react(const FatalError& err) {
  if (!is_in_state<states::Error>()) {
    transit<states::Error>();
  }
}

}  // namespace system_fsm

FSM_INITIAL_STATE(system_fsm::SystemState, system_fsm::states::Booting)
