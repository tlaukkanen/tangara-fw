/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "adc.hpp"
#include "samd.hpp"

namespace battery {

class Battery {
 public:
  Battery(drivers::Samd* samd, drivers::AdcBattery* adc);
  ~Battery();

  auto Update() -> void;

  struct BatteryState {
    uint_fast8_t percent;
    bool is_charging;
  };

  auto State() -> std::optional<BatteryState>;

 private:
  auto EmitEvent() -> void;

  drivers::Samd* samd_;
  drivers::AdcBattery* adc_;

  TimerHandle_t timer_;
  std::mutex state_mutex_;
  std::optional<BatteryState> state_;
};

}  // namespace battery
