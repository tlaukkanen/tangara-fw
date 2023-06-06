/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <functional>

#include "esp_err.h"
#include "result.hpp"

#include "gpio_expander.hpp"

namespace drivers {

/*
 * Driver for a two-channel digital potentiometer, with steps measured in
 * decibels.
 */
class DigitalPot {
 public:
  explicit DigitalPot(GpioExpander* gpios);
  ~DigitalPot() {}

  // Not copyable or movable.
  DigitalPot(const DigitalPot&) = delete;
  DigitalPot& operator=(const DigitalPot&) = delete;

  enum class Channel {
    kLeft,
    kRight,
  };

  auto SetRelative(int_fast8_t change) -> void;
  auto SetRelative(Channel ch, int_fast8_t change) -> void;

  auto SetZeroCrossDetect(bool enabled) -> void;

  auto GetMaxAttenuation() -> int_fast8_t;
  auto GetMinAttenuation() -> int_fast8_t;

 private:
  GpioExpander* gpios_;
};

}  // namespace drivers
