/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_touch_wheel.hpp"
#include <stdint.h>

#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "haptics.hpp"
#include "input_device.hpp"
#include "touchwheel.hpp"

namespace input {

TouchWheel::TouchWheel(drivers::TouchWheel& wheel)
    : wheel_(wheel),
      is_scrolling_(false),
      sensitivity_(128),
      threshold_(10),
      is_first_read_(true),
      last_angle_(0) {}

auto TouchWheel::read(lv_indev_data_t* data) -> void {
  wheel_.Update();
  auto wheel_data = wheel_.GetTouchWheelData();
  int8_t ticks = calculate_ticks(wheel_data);

  if (!wheel_data.is_wheel_touched) {
    // User has released the wheel.
    is_scrolling_ = false;
    data->enc_diff = 0;
  } else if (ticks != 0) {
    // User is touching the wheel, and has just passed the sensitivity
    // threshold for a scroll tick.
    is_scrolling_ = true;
    data->enc_diff = ticks;
  } else {
    // User is touching the wheel, but hasn't moved.
    data->enc_diff = 0;
  }

  if (!is_scrolling_ && wheel_data.is_button_touched) {
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

auto TouchWheel::calculate_ticks(const drivers::TouchWheelData& data)
    -> int8_t {
  if (!data.is_wheel_touched) {
    is_first_read_ = true;
    return 0;
  }

  uint8_t new_angle = data.wheel_position;
  if (is_first_read_) {
    is_first_read_ = false;
    last_angle_ = new_angle;
    return 0;
  }

  int delta = 128 - last_angle_;
  uint8_t rotated_angle = new_angle + delta;
  if (rotated_angle < 128 - threshold_) {
    last_angle_ = new_angle;
    return 1;
  } else if (rotated_angle > 128 + threshold_) {
    last_angle_ = new_angle;
    return -1;
  } else {
    return 0;
  }
}

}  // namespace input
