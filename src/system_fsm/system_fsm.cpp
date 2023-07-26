/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_fsm.hpp"
#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "relative_wheel.hpp"
#include "system_events.hpp"
#include "tag_parser.hpp"
#include "track_queue.hpp"

namespace system_fsm {

std::shared_ptr<drivers::Gpios> SystemState::sGpios;
std::shared_ptr<drivers::Samd> SystemState::sSamd;

std::shared_ptr<drivers::TouchWheel> SystemState::sTouch;
std::shared_ptr<drivers::RelativeWheel> SystemState::sRelativeTouch;
std::shared_ptr<drivers::Battery> SystemState::sBattery;
std::shared_ptr<drivers::SdStorage> SystemState::sStorage;
std::shared_ptr<drivers::Display> SystemState::sDisplay;

std::shared_ptr<database::Database> SystemState::sDatabase;
std::shared_ptr<database::TagParserImpl> SystemState::sTagParser;

std::shared_ptr<audio::TrackQueue> SystemState::sTrackQueue;

console::AppConsole* SystemState::sAppConsole;

auto SystemState::early_init_gpios() -> drivers::Gpios* {
  sGpios.reset(drivers::Gpios::Create());
  return sGpios.get();
}

void SystemState::react(const FatalError& err) {
  if (!is_in_state<states::Error>()) {
    transit<states::Error>();
  }
}

void SystemState::react(const internal::GpioInterrupt&) {
  bool prev_key_up = sGpios->Get(drivers::Gpios::Pin::kKeyUp);
  bool prev_key_down = sGpios->Get(drivers::Gpios::Pin::kKeyDown);
  bool prev_key_lock = sGpios->Get(drivers::Gpios::Pin::kKeyLock);
  bool prev_has_headphones = !sGpios->Get(drivers::Gpios::Pin::kPhoneDetect);

  sGpios->Read();

  bool key_up = sGpios->Get(drivers::Gpios::Pin::kKeyUp);
  bool key_down = sGpios->Get(drivers::Gpios::Pin::kKeyDown);
  bool key_lock = sGpios->Get(drivers::Gpios::Pin::kKeyLock);
  bool has_headphones = !sGpios->Get(drivers::Gpios::Pin::kPhoneDetect);

  if (key_up != prev_key_up) {
    KeyUpChanged ev{.falling = prev_key_up};
    events::Audio().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (key_down != prev_key_down) {
    KeyDownChanged ev{.falling = prev_key_up};
    events::Audio().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (key_lock != prev_key_lock) {
    KeyLockChanged ev{.falling = prev_key_up};
    events::System().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (has_headphones != prev_has_headphones) {
    HasPhonesChanged ev{.falling = prev_key_up};
    events::Audio().Dispatch(ev);
  }
}

}  // namespace system_fsm

FSM_INITIAL_STATE(system_fsm::SystemState, system_fsm::states::Booting)
