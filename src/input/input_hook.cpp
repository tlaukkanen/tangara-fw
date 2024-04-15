/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_hook.hpp"

#include <functional>
#include <optional>
#include "hal/lv_hal_indev.h"

namespace input {

Hook::Hook(HookCb fn) : default_(fn), override_() {}

auto Hook::invoke(lv_indev_data_t* d) -> void {
  if (override_) {
    std::invoke(*override_, d);
  } else if (default_) {
    std::invoke(*default_, d);
  }
}

auto Hook::override(HookCb fn) -> void {
  override_ = fn;
}

TriggerHooks::TriggerHooks(HookCb click, HookCb long_press, HookCb repeat)
    : click_(click), long_press_(long_press), repeat_(repeat) {}

auto TriggerHooks::update(bool pressed, lv_indev_data_t* d) -> void {
  switch (trigger_.update(pressed)) {
    case Trigger::State::kClick:
      click_.invoke(d);
      break;
    case Trigger::State::kLongPress:
      long_press_.invoke(d);
      break;
    case Trigger::State::kRepeatPress:
      repeat_.invoke(d);
      break;
    case Trigger::State::kNone:
    default:
      break;
  }
}

auto TriggerHooks::override(Trigger::State s, HookCb fn) -> void {
  switch (s) {
    case Trigger::State::kClick:
      click_.override(fn);
      break;
    case Trigger::State::kLongPress:
      long_press_.override(fn);
      break;
    case Trigger::State::kRepeatPress:
      repeat_.override(fn);
      break;
    case Trigger::State::kNone:
    default:
      break;
  }
}

}  // namespace input
