/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/input_touch_dpad.hpp"

#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "drivers/haptics.hpp"
#include "drivers/touchwheel.hpp"

#include "events/event_queue.hpp"
#include "input/input_device.hpp"
#include "input/input_hook_actions.hpp"
#include "input/input_touch_dpad.hpp"

namespace input {

TouchDPad::TouchDPad(drivers::TouchWheel& wheel)
    : wheel_(wheel),
      centre_("centre", actions::select(), {}, {}, {}),
      up_("up", actions::scrollUp(), {}, {}, actions::scrollUp()),
      right_("right", actions::select(), {}, {}, {}),
      down_("down", actions::scrollDown(), {}, {}, actions::scrollDown()),
      left_("left", actions::goBack(), {}, {}, {}) {}

auto TouchDPad::read(lv_indev_data_t* data) -> void {
  wheel_.Update();
  auto wheel_data = wheel_.GetTouchWheelData();

  centre_.update(wheel_data.is_button_touched, data);

  up_.update(
      wheel_data.is_wheel_touched &&
          drivers::TouchWheel::isAngleWithin(wheel_data.wheel_position, 0, 32),
      data);
  right_.update(
      wheel_data.is_wheel_touched && drivers::TouchWheel::isAngleWithin(
                                         wheel_data.wheel_position, 192, 32),
      data);
  down_.update(
      wheel_data.is_wheel_touched && drivers::TouchWheel::isAngleWithin(
                                         wheel_data.wheel_position, 128, 32),
      data);
  left_.update(
      wheel_data.is_wheel_touched &&
          drivers::TouchWheel::isAngleWithin(wheel_data.wheel_position, 64, 32),
      data);
}

auto TouchDPad::name() -> std::string {
  return "dpad";
}

auto TouchDPad::triggers()
    -> std::vector<std::reference_wrapper<TriggerHooks>> {
  return {centre_, up_, right_, down_, left_};
}

}  // namespace input
