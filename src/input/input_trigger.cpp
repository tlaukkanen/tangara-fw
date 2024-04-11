/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_trigger.hpp"
#include <sys/_stdint.h>

#include <cstdint>
#include "esp_log.h"
#include "esp_timer.h"

namespace input {

Trigger::Trigger() : touch_time_ms_(), times_fired_(0) {}

auto Trigger::update(bool is_pressed) -> State {
  // Bail out early if we're in a steady-state of not pressed.
  if (!is_pressed && !touch_time_ms_) {
    return State::kNone;
  }

  uint64_t now_ms = esp_timer_get_time() / 1000;

  // Initial press of this key: record the current time, and report that we
  // haven't triggered yet.
  if (is_pressed && !touch_time_ms_) {
    touch_time_ms_ = now_ms;
    times_fired_ = 0;
    return State::kNone;
  }

  // The key was released. If there were no long-press events fired during the
  // press, then this was a standard click.
  if (!is_pressed && touch_time_ms_) {
    touch_time_ms_.reset();
    if (times_fired_ == 0) {
      return State::kClick;
    } else {
      return State::kNone;
    }
  }

  // Now the more complicated case: the user is continuing to press the button.
  if (times_fired_ == 0) {
    // We haven't fired yet, so we wait for the long-press event.
    if (now_ms - *touch_time_ms_ >= kLongPressDelayMs) {
      times_fired_++;
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
    if (times_fired_ < expected_times_fired) {
      times_fired_++;
      return State::kRepeatPress;
    }
  }

  return State::kNone;
}

}  // namespace input
