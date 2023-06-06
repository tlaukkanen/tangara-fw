/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <functional>

#include "esp_err.h"
#include "result.hpp"

#include "gpio_expander.hpp"
#include "touchwheel.hpp"

namespace drivers {

class RelativeWheel {
 public:
  static auto Create(TouchWheel* touch) -> RelativeWheel* {
    return new RelativeWheel(touch);
  }

  explicit RelativeWheel(TouchWheel* touch);

  // Not copyable or movable.
  RelativeWheel(const RelativeWheel&) = delete;
  RelativeWheel& operator=(const RelativeWheel&) = delete;

  auto Update() -> void;

  auto is_pressed() -> bool;
  auto ticks() -> std::int_fast16_t;

 private:
  TouchWheel* touch_;
  bool is_pressed_;
  bool is_first_read_;
  std::int_fast16_t ticks_;
  uint8_t last_angle_;
};

}  // namespace drivers
