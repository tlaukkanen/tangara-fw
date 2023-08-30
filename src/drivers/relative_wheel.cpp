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
      is_clicking_(false),
      was_clicking_(false),
      is_first_read_(true),
      ticks_(0),
      last_angle_(0) {}

auto RelativeWheel::Update() -> void {
  touch_.Update();
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
  int threshold = 15;

  if (rotated_angle < 128 - threshold) {
    ticks_ = 1;
    last_angle_ = new_angle;
  } else if (rotated_angle > 128 + threshold) {
    ticks_ = -1;
    last_angle_ = new_angle;
  } else {
    ticks_ = 0;
  }
}

auto RelativeWheel::SetEnabled(bool en) -> void {
  is_enabled_ = en;
}

auto RelativeWheel::is_clicking() -> bool {
  if (!is_enabled_) {
    return false;
  }
  bool ret = is_clicking_;
  is_clicking_ = 0;
  return ret;
}

auto RelativeWheel::ticks() -> std::int_fast16_t {
  if (!is_enabled_) {
    return 0;
  }
  int_fast16_t t = ticks_;
  ticks_ = 0;
  return t;
}

}  // namespace drivers
