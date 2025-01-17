/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_fsm/system_fsm.hpp"
#include "audio/audio_fsm.hpp"
#include "audio/track_queue.hpp"
#include "database/tag_parser.hpp"
#include "driver/gpio.h"
#include "drivers/gpios.hpp"
#include "events/event_queue.hpp"
#include "system_fsm/service_locator.hpp"
#include "system_fsm/system_events.hpp"

[[maybe_unused]] static const char kTag[] = "system";

namespace system_fsm {

std::shared_ptr<ServiceLocator> SystemState::sServices;
std::unique_ptr<drivers::SdStorage> SystemState::sStorage;

console::AppConsole* SystemState::sAppConsole;

void check_interrupts_cb(TimerHandle_t timer) {
  if (!gpio_get_level(GPIO_NUM_34)) {
    events::System().Dispatch(internal::GpioInterrupt{});
  }
  if (!gpio_get_level(GPIO_NUM_35)) {
    events::System().Dispatch(internal::SamdInterrupt{});
  }
}

void SystemState::react(const FatalError& err) {
  if (!is_in_state<states::Error>()) {
    transit<states::Error>();
  }
}

void SystemState::react(const HapticTrigger& trigger) {
  auto& haptics = sServices->haptics();
  haptics.PlayWaveformEffect(trigger.effect);
}

void SystemState::react(const internal::GpioInterrupt&) {
  auto& gpios = sServices->gpios();
  bool prev_key_lock = gpios.IsLocked();
  bool prev_has_headphones = !gpios.Get(drivers::Gpios::Pin::kPhoneDetect);
  bool prev_has_sd = gpios.Get(drivers::Gpios::Pin::kSdCardDetect);

  gpios.Read();

  bool key_lock = gpios.IsLocked();
  bool has_headphones = !gpios.Get(drivers::Gpios::Pin::kPhoneDetect);
  bool has_sd = gpios.Get(drivers::Gpios::Pin::kSdCardDetect);

  if (key_lock != prev_key_lock) {
    KeyLockChanged ev{.locking = key_lock};
    events::System().Dispatch(ev);
    events::Audio().Dispatch(ev);
    events::Ui().Dispatch(ev);
  }
  if (has_headphones != prev_has_headphones) {
    HasPhonesChanged ev{.has_headphones = has_headphones};
    events::Audio().Dispatch(ev);
  }
  if (has_sd != prev_has_sd) {
    SdDetectChanged ev{.has_sd_card = !has_sd};
    events::System().Dispatch(ev);
    events::Ui().Dispatch(ev);
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

  if (charge_status != prev_charge_status && sServices) {
    sServices->battery().Update();
  }
  if (usb_status != prev_usb_status) {
    events::Ui().Dispatch(SamdUsbStatusChanged{.new_status = usb_status});
  }
}

auto SystemState::IdleCondition() -> bool {
  auto db = sServices->database().lock();
  return sServices->gpios().IsLocked() && !(db && db->isUpdating()) &&
         audio::AudioState::is_in_state<audio::states::Standby>();
}

}  // namespace system_fsm

FSM_INITIAL_STATE(system_fsm::SystemState, system_fsm::states::Booting)
