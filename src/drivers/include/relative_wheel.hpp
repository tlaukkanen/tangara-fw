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

#include "gpios.hpp"
#include "touchwheel.hpp"

namespace drivers {

class RelativeWheel {
 public:
  explicit RelativeWheel(TouchWheel& touch);

  auto Update() -> void;
  auto SetEnabled(bool) -> void;

  auto SetSensitivity(uint8_t) -> void;
  auto GetSensitivity() -> uint8_t;

  auto is_clicking() const -> bool;
  auto ticks() const -> std::int_fast16_t;

  // Not copyable or movable.
  RelativeWheel(const RelativeWheel&) = delete;
  RelativeWheel& operator=(const RelativeWheel&) = delete;

 private:
  TouchWheel& touch_;

  bool is_enabled_;
  uint8_t sensitivity_;
  uint8_t threshold_;

  bool is_clicking_;
  bool was_clicking_;
  bool is_first_read_;
  std::int_fast16_t ticks_;
  uint8_t last_angle_;
};

}  // namespace drivers
