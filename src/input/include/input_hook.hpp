/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <optional>
#include <string>

#include "hal/lv_hal_indev.h"
#include "lua.hpp"

#include "input_trigger.hpp"

namespace input {

struct HookCallback {
  std::string name;
  std::function<void(lv_indev_data_t*)> fn;
};

class Hook {
 public:
  Hook(std::string name, std::optional<HookCallback> cb);

  auto invoke(lv_indev_data_t*) -> void;
  auto override(std::optional<HookCallback>) -> void;

  auto name() const -> const std::string& { return name_; }
  auto callback() -> std::optional<HookCallback>;

 private:
  std::string name_;
  std::optional<HookCallback> default_;
  std::optional<HookCallback> override_;
};

class TriggerHooks {
 public:
  TriggerHooks(std::string name, std::optional<HookCallback> cb)
      : TriggerHooks(name, cb, cb, cb) {}
  TriggerHooks(std::string name,
               std::optional<HookCallback> click,
               std::optional<HookCallback> long_press,
               std::optional<HookCallback> repeat);

  auto update(bool, lv_indev_data_t*) -> void;
  auto override(Trigger::State, std::optional<HookCallback>) -> void;

  auto name() const -> const std::string&;
  auto pushHooks(lua_State*) -> void;

 private:
  std::string name_;
  Trigger trigger_;

  Hook click_;
  Hook long_press_;
  Hook repeat_;
};

}  // namespace input
