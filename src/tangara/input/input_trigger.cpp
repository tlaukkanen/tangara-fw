/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/input_trigger.hpp"

#include <cstdint>

#include "esp_timer.h"

namespace input {

Trigger::Trigger()
    : touch_time_ms_(),
      was_pressed_(false),
      was_double_click_(false),
      times_long_pressed_(0) {}

auto Trigger::update(bool is_pressed) -> State {
  // Bail out early if we're in a steady-state of not pressed.
  if (!is_pressed && !was_pressed_) {
    was_double_click_ = false;
    times_long_pressed_ = 0;
    return State::kNone;
  }

  uint64_t now_ms = esp_timer_get_time() / 1000;

  // This key wasn't being pressed, but now it is.
  if (is_pressed && !was_pressed_) {
    // Is this a double click?
    if (now_ms - *touch_time_ms_ < kDoubleClickDelayMs) {
      // Don't update touch_time_ms_, since we don't want triple clicks to
      // register as double clicks.
      was_double_click_ = true;
      was_pressed_ = true;
      return State::kDoubleClick;
    }
    // Not a double click; update our accounting info and wait for the next
    // call.
    touch_time_ms_ = now_ms;
    was_double_click_ = false;
    times_long_pressed_ = 0;
    was_pressed_ = true;
    return State::kNone;
  }

  // The key was released. If there were no long-press events fired during the
  // press, then this was a standard click.
  if (!is_pressed && was_pressed_) {
    was_pressed_ = false;
    if (!was_double_click_ && times_long_pressed_ == 0) {
      return State::kClick;
    } else {
      return State::kNone;
    }
  }

  // Now the more complicated case: the user is continuing to press the button.
  if (times_long_pressed_ == 0) {
    // We haven't fired yet, so we wait for the long-press event.
    if (now_ms - *touch_time_ms_ >= kLongPressDelayMs) {
      times_long_pressed_++;
      return State::kLongPress;
    }
  } else {
    // We've already fired at least once. How long has the user been holding
    // the key for?
    uint64_t time_since_long_press =
        now_ms - (*touch_time_ms_ + kLongPressDelayMs);

    // How many times should we have fired?
    // 1 initial fire (for the long-press), plus one additional fire every
    // kRepeatDelayMs since the long-press event.
    uint16_t expected_times_fired =
        1 + (time_since_long_press / kRepeatDelayMs);
    if (times_long_pressed_ < expected_times_fired) {
      times_long_pressed_++;
      return State::kRepeatPress;
    }
  }

  return State::kNone;
}

}  // namespace input
