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
#include "service_locator.hpp"
#include "system_events.hpp"
#include "tag_parser.hpp"
#include "track_queue.hpp"

[[maybe_unused]] static const char kTag[] = "system";

namespace system_fsm {

std::shared_ptr<ServiceLocator> SystemState::sServices;
std::unique_ptr<drivers::SdStorage> SystemState::sStorage;

console::AppConsole* SystemState::sAppConsole;

void SystemState::react(const FatalError& err) {
  if (!is_in_state<states::Error>()) {
    transit<states::Error>();
  }
}

void SystemState::react(const HapticTrigger& trigger) {
  auto& haptics = sServices->haptics();
  haptics.SetWaveformEffect(trigger.effect);
  haptics.Go();
}

void SystemState::react(const internal::GpioInterrupt&) {
  auto& gpios = sServices->gpios();
  bool prev_key_lock = gpios.Get(drivers::Gpios::Pin::kKeyLock);
  bool prev_has_headphones = !gpios.Get(drivers::Gpios::Pin::kPhoneDetect);

  gpios.Read();

  bool key_lock = gpios.Get(drivers::Gpios::Pin::kKeyLock);
  bool has_headphones = !gpios.Get(drivers::Gpios::Pin::kPhoneDetect);

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
  auto& samd = sServices->samd();
  auto prev_charge_status = samd.GetChargeStatus();
  auto prev_usb_status = samd.GetUsbStatus();

  samd.UpdateChargeStatus();
  samd.UpdateUsbStatus();

  auto charge_status = samd.GetChargeStatus();
  auto usb_status = samd.GetUsbStatus();

  if (charge_status != prev_charge_status) {
    ChargingStatusChanged ev{};
    events::System().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (usb_status != prev_usb_status) {
    ESP_LOGI(kTag, "usb status changed");
  }
}

auto SystemState::IdleCondition() -> bool {
  return !sServices->gpios().Get(drivers::IGpios::Pin::kKeyLock) &&
         audio::AudioState::is_in_state<audio::states::Standby>();
}

}  // namespace system_fsm

FSM_INITIAL_STATE(system_fsm::SystemState, system_fsm::states::Booting)
