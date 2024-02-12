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

RelativeWheel::RelativeWheel(TouchWheel& touch)
    : touch_(touch),
      is_enabled_(true),
      threshold_(10),
      is_clicking_(false),
      was_clicking_(false),
      is_first_read_(true),
      ticks_(0),
      last_angle_(0) {}

auto RelativeWheel::Update() -> void {
  TouchWheelData d = touch_.GetTouchWheelData();

  is_clicking_ = d.is_button_touched;

  if (is_clicking_) {
    ticks_ = 0;
    return;
  }

  if (!d.is_wheel_touched) {
    ticks_ = 0;
    is_first_read_ = true;
    return;
  }

  uint8_t new_angle = d.wheel_position;
  if (is_first_read_) {
    is_first_read_ = false;
    last_angle_ = new_angle;
    return;
  }

  int delta = 128 - last_angle_;
  uint8_t rotated_angle = new_angle + delta;

  if (rotated_angle < 128 - threshold_) {
    ticks_ = 1;
    last_angle_ = new_angle;
  } else if (rotated_angle > 128 + threshold_) {
    ticks_ = -1;
    last_angle_ = new_angle;
  } else {
    ticks_ = 0;
  }
}

auto RelativeWheel::SetEnabled(bool en) -> void {
  is_enabled_ = en;
}

auto RelativeWheel::SetThreshold(int val) -> void {
  threshold_ = val;
}

auto RelativeWheel::GetThreshold() -> int {
  return threshold_;
}

auto RelativeWheel::is_clicking() const -> bool {
  if (!is_enabled_) {
    return false;
  }
  return is_clicking_;
}

auto RelativeWheel::ticks() const -> std::int_fast16_t {
  if (!is_enabled_) {
    return 0;
  }
  return ticks_;
}

}  // namespace drivers
