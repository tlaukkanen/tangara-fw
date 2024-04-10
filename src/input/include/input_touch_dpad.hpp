/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "haptics.hpp"
#include "input_device.hpp"
#include "touchwheel.hpp"

namespace input {

class TouchDPad : public IInputDevice {
 public:
  TouchDPad(drivers::TouchWheel&);

  auto read(lv_indev_data_t* data) -> void override;

 private:
  drivers::TouchWheel& wheel_;
};

}  // namespace input
