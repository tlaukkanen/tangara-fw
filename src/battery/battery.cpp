/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "battery.hpp"

#include <cstdint>

#include "adc.hpp"
#include "event_queue.hpp"
#include "freertos/portmacro.h"
#include "samd.hpp"
#include "system_events.hpp"

namespace battery {

static const TickType_t kBatteryCheckPeriod = pdMS_TO_TICKS(60 * 1000);

/*
 * Battery voltage, in millivolts, at which the battery charger IC will stop
 * charging.
 */
static const uint32_t kFullChargeMilliVolts = 4200;

/*
 * Battery voltage, in millivolts, at which *we* will consider the battery to
 * be completely discharged. This is intentionally higher than the charger IC
 * cut-off and the protection on the battery itself; we want to make sure we
 * finish up and have everything unmounted and snoozing before the BMS cuts us
 * off.
 */
static const uint32_t kEmptyChargeMilliVolts = 3200;  // BMS limit is 3100.

using ChargeStatus = drivers::Samd::ChargeStatus;

void check_voltage_cb(TimerHandle_t timer) {
  Battery* instance = reinterpret_cast<Battery*>(pvTimerGetTimerID(timer));
  instance->Update();
}

Battery::Battery(drivers::Samd& samd, std::unique_ptr<drivers::AdcBattery> adc)
    : samd_(samd), adc_(std::move(adc)) {
  timer_ = xTimerCreate("BATTERY", kBatteryCheckPeriod, true, this,
                        check_voltage_cb);
  xTimerStart(timer_, portMAX_DELAY);
  Update();
}

Battery::~Battery() {
  xTimerStop(timer_, portMAX_DELAY);
  xTimerDelete(timer_, portMAX_DELAY);
}

auto Battery::Update() -> void {
  std::lock_guard<std::mutex> lock{state_mutex_};

  auto charge_state = samd_.GetChargeStatus();
  if (!charge_state || *charge_state == ChargeStatus::kNoBattery) {
    if (state_) {
      EmitEvent();
    }
    state_.reset();
    return;
  }
  // FIXME: So what we *should* do here is measure the actual real-life
  // time from full battery -> empty battery, store it in NVS, then rely on
  // that. If someone could please do this, it would be lovely. Thanks!
  uint32_t mV = std::max(adc_->Millivolts(), kEmptyChargeMilliVolts);
  uint_fast8_t percent = static_cast<uint_fast8_t>(std::min<double>(
      std::max<double>(0.0, mV - kEmptyChargeMilliVolts) /
          (kFullChargeMilliVolts - kEmptyChargeMilliVolts) * 100.0,
      100.0));

  bool is_charging = *charge_state == ChargeStatus::kChargingRegular ||
                     *charge_state == ChargeStatus::kChargingFast ||
                     *charge_state == ChargeStatus::kFullCharge;

  if (state_ && state_->is_charging == is_charging &&
      state_->percent == percent) {
    return;
  }

  state_ = BatteryState{
      .percent = percent,
      .millivolts = mV,
      .is_charging = is_charging,
  };
  EmitEvent();
}

auto Battery::State() -> std::optional<BatteryState> {
  std::lock_guard<std::mutex> lock{state_mutex_};
  return state_;
}

auto Battery::EmitEvent() -> void {
  auto state = state_;
  if (!state) {
    return;
  }
  system_fsm::BatteryStateChanged ev{
      .new_state = *state,
  };
  events::System().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

}  // namespace battery
