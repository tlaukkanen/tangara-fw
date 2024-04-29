/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_hook.hpp"

#include <functional>
#include <optional>
#include "hal/lv_hal_indev.h"
#include "input_trigger.hpp"
#include "lua.h"

namespace input {

Hook::Hook(std::string name, std::optional<HookCallback> cb)
    : name_(name), default_(cb), override_() {}

auto Hook::invoke(lv_indev_data_t* d) -> void {
  auto cb = callback();
  if (cb) {
    std::invoke(cb->fn, d);
  }
}

auto Hook::override(std::optional<HookCallback> cb) -> void {
  override_ = cb;
}

auto Hook::callback() -> std::optional<HookCallback> {
  if (override_) {
    return override_;
  } else if (default_) {
    return default_;
  }
  return {};
}

TriggerHooks::TriggerHooks(std::string name,
                           std::optional<HookCallback> click,
                           std::optional<HookCallback> double_click,
                           std::optional<HookCallback> long_press,
                           std::optional<HookCallback> repeat)
    : name_(name),
      click_("click", click),
      double_click_("double_click", double_click),
      long_press_("long_press", long_press),
      repeat_("repeat", repeat) {}

auto TriggerHooks::update(bool pressed, lv_indev_data_t* d) -> void {
  switch (trigger_.update(pressed)) {
    case Trigger::State::kClick:
      click_.invoke(d);
      break;
    case Trigger::State::kDoubleClick:
      double_click_.invoke(d);
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

auto TriggerHooks::override(Trigger::State s, std::optional<HookCallback> cb)
    -> void {
  switch (s) {
    case Trigger::State::kClick:
      click_.override(cb);
      break;
    case Trigger::State::kLongPress:
      long_press_.override(cb);
      break;
    case Trigger::State::kRepeatPress:
      repeat_.override(cb);
      break;
    case Trigger::State::kNone:
    default:
      break;
  }
}

auto TriggerHooks::name() const -> const std::string& {
  return name_;
}

auto TriggerHooks::hooks() -> std::vector<std::reference_wrapper<Hook>> {
  return {click_, long_press_, repeat_};
}

}  // namespace input
