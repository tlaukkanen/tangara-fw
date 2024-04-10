/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "gpios.hpp"
#include "hal/lv_hal_indev.h"

#include "haptics.hpp"
#include "input_device.hpp"
#include "touchwheel.hpp"

namespace input {

class VolumeButtons : public IInputDevice {
 public:
  VolumeButtons(drivers::IGpios&);

  auto read(lv_indev_data_t* data) -> void override;

 private:
  drivers::IGpios& gpios_;
};

}  // namespace input
