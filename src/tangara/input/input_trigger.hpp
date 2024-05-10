/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <optional>

#include "hal/lv_hal_indev.h"

namespace input {

const uint16_t kDoubleClickDelayMs = 500;
const uint16_t kLongPressDelayMs = LV_INDEV_DEF_LONG_PRESS_TIME;
const uint16_t kRepeatDelayMs = LV_INDEV_DEF_LONG_PRESS_REP_TIME;

class Trigger {
 public:
  enum class State {
    kNone,
    kClick,
    kDoubleClick,
    kLongPress,
    kRepeatPress,
  };

  Trigger();

  auto update(bool is_pressed) -> State;
  auto cancel() -> void;

 private:
  std::optional<uint64_t> touch_time_ms_;
  bool was_pressed_;

  bool was_double_click_;
  uint16_t times_long_pressed_;
};

}  // namespace input
