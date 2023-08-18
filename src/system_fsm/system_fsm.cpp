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

static const char kTag[] = "system";

namespace system_fsm {

std::shared_ptr<drivers::Gpios> SystemState::sGpios;
std::shared_ptr<drivers::Samd> SystemState::sSamd;
std::shared_ptr<drivers::NvsStorage> SystemState::sNvs;

std::shared_ptr<drivers::TouchWheel> SystemState::sTouch;
std::shared_ptr<drivers::RelativeWheel> SystemState::sRelativeTouch;
std::shared_ptr<drivers::Battery> SystemState::sBattery;
std::shared_ptr<drivers::SdStorage> SystemState::sStorage;
std::shared_ptr<drivers::Display> SystemState::sDisplay;
std::shared_ptr<drivers::Bluetooth> SystemState::sBluetooth;

std::shared_ptr<database::Database> SystemState::sDatabase;
std::shared_ptr<database::TagParserImpl> SystemState::sTagParser;

std::shared_ptr<audio::TrackQueue> SystemState::sTrackQueue;

console::AppConsole* SystemState::sAppConsole;

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
    KeyDownChanged ev{.falling = prev_key_down};
    events::Audio().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (key_lock != prev_key_lock) {
    KeyLockChanged ev{.falling = key_lock};
    events::System().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (has_headphones != prev_has_headphones) {
    HasPhonesChanged ev{.falling = prev_has_headphones};
    events::Audio().Dispatch(ev);
  }
}

void SystemState::react(const internal::SamdInterrupt&) {
  auto prev_charge_status = sSamd->GetChargeStatus();
  auto prev_usb_status = sSamd->GetUsbStatus();

  sSamd->UpdateChargeStatus();
  sSamd->UpdateUsbStatus();

  auto charge_status = sSamd->GetChargeStatus();
  auto usb_status = sSamd->GetUsbStatus();

  if (charge_status != prev_charge_status) {
    ChargingStatusChanged ev{};
    events::System().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (usb_status != prev_usb_status) {
    ESP_LOGI(kTag, "usb status changed");
  }
}

}  // namespace system_fsm

FSM_INITIAL_STATE(system_fsm::SystemState, system_fsm::states::Booting)
