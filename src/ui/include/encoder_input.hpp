/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "core/lv_group.h"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"

#include "relative_wheel.hpp"
#include "touchwheel.hpp"

namespace ui {

/*
 * Main input device abstracting that handles turning lower-level input device
 * drivers into events and LVGL inputs.
 *
 * As far as LVGL is concerned, this class represents an ordinary rotary
 * encoder, supporting only left and right ticks, and clicking.
 */
class EncoderInput {
 public:
  EncoderInput(drivers::IGpios& gpios, drivers::TouchWheel& wheel);

  auto Read(lv_indev_data_t* data) -> void;
  auto registration() -> lv_indev_t* { return registration_; }

  auto lock(bool l) -> void { is_locked_ = l; }

 private:
  lv_indev_drv_t driver_;
  lv_indev_t* registration_;

  drivers::IGpios& gpios_;
  drivers::TouchWheel& raw_wheel_;
  std::unique_ptr<drivers::RelativeWheel> relative_wheel_;

  bool is_locked_;
};

}  // namespace ui
