/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <optional>

namespace input {

const uint16_t kDoubleClickDelayMs = 500;
const uint16_t kLongPressDelayMs = 400;
const uint16_t kRepeatDelayMs = 100;

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
