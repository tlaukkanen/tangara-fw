/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_touch_dpad.hpp"

#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "event_queue.hpp"
#include "haptics.hpp"
#include "input_device.hpp"
#include "input_touch_dpad.hpp"
#include "touchwheel.hpp"

namespace input {

static inline auto IsAngleWithin(int16_t wheel_angle,
                                 int16_t target_angle,
                                 int threshold) -> bool {
  int16_t difference = (wheel_angle - target_angle + 127 + 255) % 255 - 127;
  return difference <= threshold && difference >= -threshold;
}

TouchDPad::TouchDPad(drivers::TouchWheel& wheel) : wheel_(wheel) {}

auto TouchDPad::read(lv_indev_data_t* data) -> void {
  wheel_.Update();
  auto wheel_data = wheel_.GetTouchWheelData();

  if (wheel_data.is_button_touched) {
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }

  switch (up_.update(
      wheel_data.is_wheel_touched &&
      drivers::TouchWheel::isAngleWithin(wheel_data.wheel_position, 0, 32))) {
    case Trigger::State::kNone:
      break;
    default:
      data->enc_diff = -1;
      break;
  }
  switch (right_.update(
      wheel_data.is_wheel_touched &&
      drivers::TouchWheel::isAngleWithin(wheel_data.wheel_position, 192, 32))) {
    default:
      break;
  }
  switch (down_.update(
      wheel_data.is_wheel_touched &&
      drivers::TouchWheel::isAngleWithin(wheel_data.wheel_position, 128, 32))) {
    case Trigger::State::kNone:
      break;
    default:
      data->enc_diff = 1;
      break;
  }
  switch (left_.update(
      wheel_data.is_wheel_touched &&
      drivers::TouchWheel::isAngleWithin(wheel_data.wheel_position, 64, 32))) {
    case Trigger::State::kLongPress:
      events::Ui().Dispatch(ui::internal::BackPressed{});
      break;
    default:
      break;
  }
}

}  // namespace input
