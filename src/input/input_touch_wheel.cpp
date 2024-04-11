/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_touch_wheel.hpp"
#include <stdint.h>

#include <cstdint>
#include <variant>

#include "event_queue.hpp"
#include "hal/lv_hal_indev.h"

#include "haptics.hpp"
#include "input_device.hpp"
#include "input_trigger.hpp"
#include "nvs.hpp"
#include "property.hpp"
#include "touchwheel.hpp"
#include "ui_events.hpp"

namespace input {

TouchWheel::TouchWheel(drivers::NvsStorage& nvs, drivers::TouchWheel& wheel)
    : nvs_(nvs),
      wheel_(wheel),
      sensitivity_(static_cast<int>(nvs.ScrollSensitivity()),
                   [&](const lua::LuaValue& val) {
                     if (!std::holds_alternative<int>(val)) {
                       return false;
                     }
                     int int_val = std::get<int>(val);
                     if (int_val < 0 || int_val > UINT8_MAX) {
                       return false;
                     }
                     nvs.ScrollSensitivity(int_val);
                     threshold_ = calculateThreshold(int_val);
                     return true;
                   }),
      is_scrolling_(false),
      threshold_(calculateThreshold(nvs.ScrollSensitivity())),
      is_first_read_(true),
      last_angle_(0) {}

auto TouchWheel::read(lv_indev_data_t* data) -> void {
  wheel_.Update();
  auto wheel_data = wheel_.GetTouchWheelData();
  int8_t ticks = calculateTicks(wheel_data);

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

  // If the user is touching the wheel but not scrolling, then they may be
  // clicking on one of the wheel's cardinal directions.
  bool pressing = wheel_data.is_wheel_touched && !is_scrolling_;

  switch (up_.update(pressing && drivers::TouchWheel::isAngleWithin(
                                     wheel_data.wheel_position, 0, 32))) {
    case Trigger::State::kLongPress:
      data->enc_diff = INT16_MIN;
      break;
    default:
      break;
  }
  switch (right_.update(pressing && drivers::TouchWheel::isAngleWithin(
                                        wheel_data.wheel_position, 192, 32))) {
    default:
      break;
  }
  switch (down_.update(pressing && drivers::TouchWheel::isAngleWithin(
                                       wheel_data.wheel_position, 128, 32))) {
    case Trigger::State::kLongPress:
      data->enc_diff = INT16_MAX;
      break;
    default:
      break;
  }
  switch (left_.update(pressing && drivers::TouchWheel::isAngleWithin(
                                       wheel_data.wheel_position, 64, 32))) {
    case Trigger::State::kLongPress:
      events::Ui().Dispatch(ui::internal::BackPressed{});
      break;
    default:
      break;
  }
}

auto TouchWheel::sensitivity() -> lua::Property& {
  return sensitivity_;
}

auto TouchWheel::calculateTicks(const drivers::TouchWheelData& data) -> int8_t {
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

auto TouchWheel::calculateThreshold(uint8_t sensitivity) -> uint8_t {
  int tmax = 35;
  int tmin = 5;
  return (((255. - sensitivity) / 255.) * (tmax - tmin) + tmin);
}

}  // namespace input
