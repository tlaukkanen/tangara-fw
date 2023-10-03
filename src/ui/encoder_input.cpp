/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "encoder_input.hpp"

#include <sys/_stdint.h>
#include <memory>

#include "core/lv_group.h"
#include "esp_timer.h"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"
#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "touchwheel.hpp"

constexpr int kDPadAngleThreshold = 20;
constexpr int kLongPressDelayMs = 500;
constexpr int kRepeatDelayMs = 250;

static inline auto IsAngleWithin(int16_t wheel_angle,
                                 int16_t target_angle,
                                 int threshold) -> bool {
  int16_t difference = (wheel_angle - target_angle + 127 + 255) % 255 - 127;
  return difference <= threshold && difference >= -threshold;
}

namespace ui {

static void encoder_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  EncoderInput* instance = reinterpret_cast<EncoderInput*>(drv->user_data);
  instance->Read(data);
}

EncoderInput::EncoderInput(drivers::IGpios& gpios, drivers::TouchWheel& wheel)
    : gpios_(gpios),
      raw_wheel_(wheel),
      relative_wheel_(std::make_unique<drivers::RelativeWheel>(wheel)),
      scroller_(std::make_unique<Scroller>()),
      mode_(drivers::NvsStorage::InputModes::kRotatingWheel) {
  lv_indev_drv_init(&driver_);
  driver_.type = LV_INDEV_TYPE_ENCODER;
  driver_.read_cb = encoder_read;
  driver_.user_data = this;

  registration_ = lv_indev_drv_register(&driver_);
}

auto EncoderInput::Read(lv_indev_data_t* data) -> void {
  raw_wheel_.Update();
  relative_wheel_->Update();
  // GPIOs updating is handled by system_fsm.

  uint64_t now_ms = esp_timer_get_time() / 1000;

  // Deal with the potential overflow of our timer.
  for (auto& it : touch_time_ms_) {
    if (it.second > now_ms) {
      // esp_timer overflowed.
      it.second = 0;
    }
  }

  // Check each button.
  HandleKey(Keys::kVolumeUp, now_ms, !gpios_.Get(drivers::IGpios::Pin::kKeyUp));
  HandleKey(Keys::kVolumeDown, now_ms,
            !gpios_.Get(drivers::IGpios::Pin::kKeyDown));

  drivers::TouchWheelData wheel_data = raw_wheel_.GetTouchWheelData();
  HandleKey(Keys::kTouchWheel, now_ms, wheel_data.is_wheel_touched);
  HandleKey(Keys::kTouchWheelCenter, now_ms, wheel_data.is_button_touched);

  HandleKey(
      Keys::kDirectionalUp, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 0, kDPadAngleThreshold));
  HandleKey(
      Keys::kDirectionalLeft, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 63, kDPadAngleThreshold));
  HandleKey(
      Keys::kDirectionalDown, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 127, kDPadAngleThreshold));
  HandleKey(
      Keys::kDirectionalRight, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 189, kDPadAngleThreshold));

  // We now have enough information to give LVGL its update.
  switch (mode_) {
    case drivers::NvsStorage::InputModes::kButtonsOnly:
      data->state = LV_INDEV_STATE_RELEASED;
      if (ShortPressTrigger(Keys::kVolumeUp)) {
        data->enc_diff = -1;
      } else if (ShortPressTrigger(Keys::kVolumeDown)) {
        data->enc_diff = 1;
      } else if (LongPressTrigger(Keys::kVolumeDown, now_ms)) {
        data->state = LV_INDEV_STATE_PRESSED;
      } else if (LongPressTrigger(Keys::kVolumeUp, now_ms)) {
        // TODO: Back button event
      }
      break;
    case drivers::NvsStorage::InputModes::kButtonsWithWheel:
      data->state = ShortPressTrigger(Keys::kTouchWheel)
                        ? LV_INDEV_STATE_PRESSED
                        : LV_INDEV_STATE_RELEASED;
      if (ShortPressTriggerRepeating(Keys::kVolumeUp, now_ms)) {
        data->enc_diff = scroller_->AddInput(now_ms, -1);
      } else if (ShortPressTriggerRepeating(Keys::kVolumeDown, now_ms)) {
        data->enc_diff = scroller_->AddInput(now_ms, 1);
      }

      if (!touch_time_ms_.contains(Keys::kVolumeDown) &&
          !touch_time_ms_.contains(Keys::kVolumeUp)) {
        data->enc_diff = scroller_->AddInput(now_ms, 0);
      }
      // TODO: Long-press events.
      break;
    case drivers::NvsStorage::InputModes::kDirectionalWheel:
      data->state = ShortPressTrigger(Keys::kTouchWheelCenter)
                        ? LV_INDEV_STATE_PRESSED
                        : LV_INDEV_STATE_RELEASED;
      if (!ShortPressTriggerRepeating(Keys::kTouchWheel, now_ms)) {
        break;
      }
      if (ShortPressTriggerRepeating(Keys::kDirectionalUp, now_ms)) {
        data->enc_diff = scroller_->AddInput(now_ms, -1);
      } else if (ShortPressTriggerRepeating(Keys::kDirectionalDown, now_ms)) {
        data->enc_diff = scroller_->AddInput(now_ms, 1);
      } else if (ShortPressTrigger(Keys::kDirectionalRight)) {
        // TODO: ???
      } else if (ShortPressTrigger(Keys::kDirectionalLeft)) {
        // TODO: Back button event.
      }

      if (!touch_time_ms_.contains(Keys::kDirectionalUp) &&
          !touch_time_ms_.contains(Keys::kDirectionalDown)) {
        data->enc_diff = scroller_->AddInput(now_ms, 0);
      }
      // TODO: Long-press events.
      break;
    case drivers::NvsStorage::InputModes::kRotatingWheel:
      if (!raw_wheel_.GetTouchWheelData().is_wheel_touched) {
        data->enc_diff = scroller_->AddInput(now_ms, 0);
      } else if (relative_wheel_->ticks() != 0) {
        data->enc_diff = scroller_->AddInput(now_ms, relative_wheel_->ticks());
      } else {
        data->enc_diff = 0;
      }
      data->state = relative_wheel_->is_clicking() ? LV_INDEV_STATE_PRESSED
                                                   : LV_INDEV_STATE_RELEASED;
      // TODO: Long-press events.
      break;
  }

  // TODO: Apply inertia / acceleration.
}

auto EncoderInput::HandleKey(Keys key, uint64_t ms, bool clicked) -> void {
  if (!clicked) {
    touch_time_ms_.erase(key);
    short_press_fired_.erase(key);
    long_press_fired_.erase(key);
    return;
  }
  if (!touch_time_ms_.contains(key)) {
    touch_time_ms_[key] = ms;
  }
}

auto EncoderInput::ShortPressTrigger(Keys key) -> bool {
  if (touch_time_ms_.contains(key) && !short_press_fired_.contains(key)) {
    short_press_fired_[key] = true;
    return true;
  }
  return false;
}

auto EncoderInput::ShortPressTriggerRepeating(Keys key, uint64_t ms) -> bool {
  if (touch_time_ms_.contains(key) &&
      (!short_press_fired_.contains(key) ||
       ms - touch_time_ms_[key] >= kRepeatDelayMs)) {
    touch_time_ms_[key] = ms;
    short_press_fired_[key] = true;
    return true;
  }
  return false;
}

auto EncoderInput::LongPressTrigger(Keys key, uint64_t ms) -> bool {
  if (touch_time_ms_.contains(key) && !long_press_fired_.contains(key) &&
      ms - touch_time_ms_[key] >= kLongPressDelayMs) {
    long_press_fired_[key] = true;
    return true;
  }
  return false;
}

auto Scroller::AddInput(uint64_t ms, int direction) -> int {
  bool dir_changed =
      ((velocity_ < 0 && direction > 0) || (velocity_ > 0 && direction < 0));
  if (direction == 0 || dir_changed) {
    last_input_ms_ = ms;
    velocity_ = 0;
    return 0;
  }
  // Decay with time
  if (last_input_ms_ > ms) {
    last_input_ms_ = 0;
  }
  uint diff = ms - last_input_ms_;
  uint diff_steps = diff / 25;
  last_input_ms_ = ms + (last_input_ms_ % 50);
  // Use powers of two for our exponential decay so we can implement decay
  // trivially via bit shifting.
  velocity_ >>= diff_steps;

  velocity_ += direction * 1000;
  if (velocity_ > 0) {
    return (velocity_ + 500) / 1000;
  } else {
    return (velocity_ - 500) / 1000;
  }
}
}  // namespace ui
