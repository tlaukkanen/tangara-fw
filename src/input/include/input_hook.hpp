/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <optional>

#include "hal/lv_hal_indev.h"
#include "input_trigger.hpp"

namespace input {

using HookCb = std::optional<std::function<void(lv_indev_data_t*)>>;

class Hook {
 public:
  Hook(HookCb);

  auto invoke(lv_indev_data_t*) -> void;
  auto override(HookCb) -> void;

 private:
  HookCb default_;
  HookCb override_;
};

class TriggerHooks {
 public:
  TriggerHooks(HookCb cb) : TriggerHooks(cb, cb, cb) {}
  TriggerHooks(HookCb, HookCb, HookCb);

  auto update(bool, lv_indev_data_t*) -> void;
  auto override(Trigger::State, HookCb) -> void;

 private:
  Trigger trigger_;

  Hook click_;
  Hook long_press_;
  Hook repeat_;
};

}  // namespace input
