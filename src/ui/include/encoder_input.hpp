/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <deque>
#include <memory>
#include <set>

#include "core/lv_group.h"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"

#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "touchwheel.hpp"

namespace ui {

class Scroller;

/*
 * Main input device abstracting that handles turning lower-level input device
 * drivers into events and LVGL inputs.
 *
 * As far as LVGL is concerned, this class represents an ordinary rotary
 * encoder, supporting only left and right ticks, and clicking.
 */
class EncoderInput {
 public:
  EncoderInput(drivers::IGpios& gpios, drivers::TouchWheel& wheel);

  auto Read(lv_indev_data_t* data) -> void;
  auto registration() -> lv_indev_t* { return registration_; }

  auto mode(drivers::NvsStorage::InputModes mode) { mode_ = mode; }
  auto lock(bool l) -> void { is_locked_ = l; }

 private:
  lv_indev_drv_t driver_;
  lv_indev_t* registration_;

  drivers::IGpios& gpios_;
  drivers::TouchWheel& raw_wheel_;
  std::unique_ptr<drivers::RelativeWheel> relative_wheel_;
  std::unique_ptr<Scroller> scroller_;

  drivers::NvsStorage::InputModes mode_;
  bool is_locked_;

  // Every kind of distinct input that we could map to an action.
  enum class Keys {
    kVolumeUp,
    kVolumeDown,
    kTouchWheel,
    kTouchWheelCenter,
    kDirectionalUp,
    kDirectionalRight,
    kDirectionalDown,
    kDirectionalLeft,
  };

  // Map from a Key, to the time that it was first touched in ms. If the key is
  // currently released, where will be no entry.
  std::unordered_map<Keys, uint64_t> touch_time_ms_;
  // Set of keys that were released during the current update.
  std::set<Keys> just_released_;
  // Set of keys that have had an event fired for them since being pressed.
  std::set<Keys> fired_;

  bool is_scrolling_wheel_;

  enum class Trigger {
    kNone,
    // Regular short-click. Triggered on release for long-pressable keys,
    // triggered on the initial press for repeatable keys.
    kClick,
    kLongPress,
  };

  enum class KeyStyle {
    kRepeat,
    kLongPress,
  };

  auto UpdateKeyState(Keys key, uint64_t ms, bool clicked) -> void;
  auto TriggerKey(Keys key, KeyStyle t, uint64_t ms) -> Trigger;
};

class Scroller {
 public:
  Scroller() : last_input_ms_(0), velocity_(0) {}

  auto AddInput(uint64_t, int) -> int;

 private:
  uint64_t last_input_ms_;
  int velocity_;
};

}  // namespace ui
