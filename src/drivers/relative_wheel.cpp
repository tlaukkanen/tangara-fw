/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "relative_wheel.hpp"

#include <stdint.h>
#include <cstdint>

#include "esp_log.h"

namespace drivers {

RelativeWheel::RelativeWheel(TouchWheel *touch) 
  :touch_(touch),
  is_pressed_(false),
  is_first_read_(true),
  ticks_(0),
  last_angle_(0) {}

auto RelativeWheel::Update() -> void {
  touch_->Update();
  TouchWheelData d = touch_->GetTouchWheelData();
  is_pressed_ = d.is_touched;

  uint8_t new_angle = d.wheel_position;
  if (is_first_read_) {
    is_first_read_ = false;
    last_angle_ = new_angle;
    return;
  }

  // Work out the magnitude of travel.
  uint8_t change_cw = last_angle_ - new_angle;
  uint8_t change_ccw = new_angle - last_angle_;
  int change = std::min(change_cw, change_ccw);

  last_angle_ = new_angle;

  // Round to eliminate noise.
  if (change <= 2) {
    ticks_ = 0;
    return;
  }

  // Quantize into ticks.
  change /= 4;

  // Clamp to reliminate more noise.
  if (change > 10) {
    change = 0;
  }

  // Work out the direction of travel.
  if (change_cw > change_ccw) {
    change *= -1;
  }

  ticks_ = change;
}

auto RelativeWheel::is_pressed() -> bool {
  return is_pressed_;
}

auto RelativeWheel::ticks() -> std::int_fast16_t {
  int_fast16_t t = ticks_;
  if (t != 0) {
  ESP_LOGI("teeks", "ticks %d", t);
  }
  ticks_ = 0;
  return t;
}


}  // namespace drivers
