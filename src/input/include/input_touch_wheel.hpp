/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "haptics.hpp"
#include "input_device.hpp"
#include "touchwheel.hpp"

namespace input {

class TouchWheel : public IInputDevice {
 public:
  TouchWheel(drivers::TouchWheel&);

  auto read(lv_indev_data_t* data) -> void override;

 private:
  auto calculate_ticks(const drivers::TouchWheelData& data) -> int8_t;

  drivers::TouchWheel& wheel_;

  bool is_scrolling_;
  uint8_t sensitivity_;
  uint8_t threshold_;
  bool is_first_read_;
  uint8_t last_angle_;
};

}  // namespace input
