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

const uint16_t kLongPressDelayMs = LV_INDEV_DEF_LONG_PRESS_TIME;
const uint16_t kRepeatDelayMs = LV_INDEV_DEF_LONG_PRESS_REP_TIME;

class Trigger {
 public:
  enum class State {
    kNone,
    kClick,
    kLongPress,
    kRepeatPress,
  };

  Trigger();

  auto update(bool is_pressed) -> State;

 private:
  std::optional<uint64_t> touch_time_ms_;
  uint16_t times_fired_;
};

}  // namespace input
