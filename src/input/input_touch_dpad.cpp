/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_touch_dpad.hpp"

#include <cstdint>

#include "hal/lv_hal_indev.h"

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

  // TODO: reimplement
}

}  // namespace input
