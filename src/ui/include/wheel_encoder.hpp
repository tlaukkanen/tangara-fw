/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "hal/lv_hal_indev.h"

#include "relative_wheel.hpp"

namespace ui {

class TouchWheelEncoder {
  public:
    explicit TouchWheelEncoder(std::weak_ptr<drivers::RelativeWheel> wheel);

    auto Read(lv_indev_data_t *data) -> void;
    auto registration() -> lv_indev_t* { return registration_; }

  private:
      lv_indev_drv_t driver_;
      lv_indev_t *registration_;
      std::weak_ptr<drivers::RelativeWheel> wheel_;
};

} // namespace ui