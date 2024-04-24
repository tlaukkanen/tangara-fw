/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "haptics.hpp"
#include "input_device.hpp"
#include "input_hook.hpp"
#include "input_trigger.hpp"
#include "nvs.hpp"
#include "property.hpp"
#include "touchwheel.hpp"

namespace input {

class TouchWheel : public IInputDevice {
 public:
  TouchWheel(drivers::NvsStorage&, drivers::TouchWheel&);

  auto read(lv_indev_data_t* data) -> void override;

  auto name() -> std::string override;
  auto hooks() -> std::vector<TriggerHooks> override;

  auto sensitivity() -> lua::Property&;

 private:
  auto calculateTicks(const drivers::TouchWheelData& data) -> int8_t;
  auto calculateThreshold(uint8_t sensitivity) -> uint8_t;

  drivers::NvsStorage& nvs_;
  drivers::TouchWheel& wheel_;

  lua::Property sensitivity_;

  TriggerHooks centre_;
  TriggerHooks up_;
  TriggerHooks right_;
  TriggerHooks down_;
  TriggerHooks left_;

  bool is_scrolling_;
  uint8_t threshold_;
  bool is_first_read_;
  uint8_t last_angle_;
};

}  // namespace input
