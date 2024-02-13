/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "encoder_input.hpp"

#include <sys/_stdint.h>
#include <memory>

#include "lvgl.h"

#include "audio_events.hpp"
#include "core/lv_event.h"
#include "core/lv_group.h"
#include "esp_timer.h"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"
#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "touchwheel.hpp"
#include "ui_events.hpp"

[[maybe_unused]] static constexpr char kTag[] = "input";

constexpr int kDPadAngleThreshold = 10;
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
      mode_(drivers::NvsStorage::InputModes::kRotatingWheel),
      is_locked_(false),
      scroll_sensitivity_(10),
      is_scrolling_wheel_(false) {
  lv_indev_drv_init(&driver_);
  driver_.type = LV_INDEV_TYPE_ENCODER;
  driver_.read_cb = encoder_read;
  driver_.user_data = this;

  registration_ = lv_indev_drv_register(&driver_);
}

auto EncoderInput::Read(lv_indev_data_t* data) -> void {
  if (is_locked_) {
    return;
  }

  lv_obj_t* active_object = nullptr;
  if (registration_ && registration_->group) {
    active_object = lv_group_get_focused(registration_->group);
  }

  raw_wheel_.Update();
  relative_wheel_->Update();
  // GPIO (for volume buttons) updating is handled by system_fsm.

  uint64_t now_ms = esp_timer_get_time() / 1000;

  // Deal with the potential overflow of our timer.
  for (auto& it : touch_time_ms_) {
    if (it.second > now_ms) {
      // esp_timer overflowed.
      it.second = 0;
    }
  }

  // Check each button.
  UpdateKeyState(Keys::kVolumeUp, now_ms,
                 !gpios_.Get(drivers::IGpios::Pin::kKeyUp));
  UpdateKeyState(Keys::kVolumeDown, now_ms,
                 !gpios_.Get(drivers::IGpios::Pin::kKeyDown));

  drivers::TouchWheelData wheel_data = raw_wheel_.GetTouchWheelData();
  UpdateKeyState(Keys::kTouchWheel, now_ms, wheel_data.is_wheel_touched);
  UpdateKeyState(Keys::kTouchWheelCenter, now_ms, wheel_data.is_button_touched);

  UpdateKeyState(
      Keys::kDirectionalUp, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 0, kDPadAngleThreshold));
  UpdateKeyState(
      Keys::kDirectionalLeft, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 63, kDPadAngleThreshold));
  UpdateKeyState(
      Keys::kDirectionalDown, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 127, kDPadAngleThreshold));
  UpdateKeyState(
      Keys::kDirectionalRight, now_ms,
      wheel_data.is_wheel_touched &&
          IsAngleWithin(wheel_data.wheel_position, 189, kDPadAngleThreshold));

  // When the wheel is being scrolled, we want to ensure that other inputs
  // involving the touchwheel don't trigger. This guards again two main issues:
  //  - hesitating when your thumb is on a cardinal direction, causing an
  //    unintentional long-press,
  //  - drifting from the outside of the wheel in a way that causes the centre
  //    key to be triggered.
  if (is_scrolling_wheel_) {
    UpdateKeyState(Keys::kTouchWheelCenter, now_ms, false);
    UpdateKeyState(Keys::kDirectionalUp, now_ms, false);
    UpdateKeyState(Keys::kDirectionalLeft, now_ms, false);
    UpdateKeyState(Keys::kDirectionalDown, now_ms, false);
    UpdateKeyState(Keys::kDirectionalRight, now_ms, false);
  }

  // Now that we've determined the correct state for all keys, we can start
  // mapping key states into actions, depending on the current control scheme.
  if (mode_ == drivers::NvsStorage::InputModes::kButtonsOnly) {
    Trigger trigger;
    data->state = LV_INDEV_STATE_RELEASED;

    trigger = TriggerKey(Keys::kVolumeUp, KeyStyle::kLongPress, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        data->enc_diff = -1;
        break;
      case Trigger::kLongPress:
        events::Ui().Dispatch(internal::BackPressed{});
        break;
    }

    trigger = TriggerKey(Keys::kVolumeDown, KeyStyle::kLongPress, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        data->enc_diff = 1;
        break;
      case Trigger::kLongPress:
        data->state = LV_INDEV_STATE_PRESSED;
        break;
    }
  } else if (mode_ == drivers::NvsStorage::InputModes::kDirectionalWheel) {
    Trigger trigger;
    trigger = TriggerKey(Keys::kTouchWheelCenter, KeyStyle::kLongPress, now_ms);
    data->state = trigger == Trigger::kClick ? LV_INDEV_STATE_PRESSED
                                             : LV_INDEV_STATE_RELEASED;

    trigger = TriggerKey(Keys::kDirectionalUp, KeyStyle::kRepeat, now_ms);
    if (trigger == Trigger::kClick) {
      data->enc_diff = scroller_->AddInput(now_ms, -1);
    }

    trigger = TriggerKey(Keys::kDirectionalDown, KeyStyle::kRepeat, now_ms);
    if (trigger == Trigger::kClick) {
      data->enc_diff = scroller_->AddInput(now_ms, 1);
    }

    trigger = TriggerKey(Keys::kDirectionalLeft, KeyStyle::kRepeat, now_ms);
    if (trigger == Trigger::kClick) {
      events::Ui().Dispatch(internal::BackPressed{});
    }

    trigger = TriggerKey(Keys::kDirectionalRight, KeyStyle::kRepeat, now_ms);
    if (trigger == Trigger::kClick) {
      // TODO: ???
    }

    // Cancel scrolling if the touchpad is released.
    if (!touch_time_ms_.contains(Keys::kDirectionalUp) &&
        !touch_time_ms_.contains(Keys::kDirectionalDown)) {
      data->enc_diff = scroller_->AddInput(now_ms, 0);
    }

    trigger = TriggerKey(Keys::kVolumeUp, KeyStyle::kRepeat, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        events::Audio().Dispatch(audio::StepUpVolume{});
        break;
      case Trigger::kLongPress:
        break;
    }

    trigger = TriggerKey(Keys::kVolumeDown, KeyStyle::kRepeat, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        events::Audio().Dispatch(audio::StepDownVolume{});
        break;
      case Trigger::kLongPress:
        break;
    }
  } else if (mode_ == drivers::NvsStorage::InputModes::kRotatingWheel) {
    if (!raw_wheel_.GetTouchWheelData().is_wheel_touched) {
      // User has released the wheel.
      is_scrolling_wheel_ = false;
      data->enc_diff = scroller_->AddInput(now_ms, 0);
    } else if (relative_wheel_->ticks() != 0) {
      // User is touching the wheel, and has just passed the sensitivity
      // threshold for a scroll tick.
      is_scrolling_wheel_ = true;
      data->enc_diff = scroller_->AddInput(now_ms, relative_wheel_->ticks());
    } else {
      // User is touching the wheel, but hasn't moved.
      data->enc_diff = 0;
    }

    Trigger trigger =
        TriggerKey(Keys::kTouchWheelCenter, KeyStyle::kLongPress, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        data->state = LV_INDEV_STATE_RELEASED;
        break;
      case Trigger::kClick:
        data->state = LV_INDEV_STATE_PRESSED;
        break;
      case Trigger::kLongPress:
        if (active_object) {
          lv_event_send(active_object, LV_EVENT_LONG_PRESSED, NULL);
        }
        break;
    }

    trigger = TriggerKey(Keys::kVolumeUp, KeyStyle::kRepeat, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        events::Audio().Dispatch(audio::StepUpVolume{});
        break;
      case Trigger::kLongPress:
        break;
    }

    trigger = TriggerKey(Keys::kVolumeDown, KeyStyle::kRepeat, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        events::Audio().Dispatch(audio::StepDownVolume{});
        break;
      case Trigger::kLongPress:
        break;
    }

    trigger = TriggerKey(Keys::kDirectionalLeft, KeyStyle::kLongPress, now_ms);
    switch (trigger) {
      case Trigger::kNone:
        break;
      case Trigger::kClick:
        break;
      case Trigger::kLongPress:
        events::Ui().Dispatch(internal::BackPressed{});
        break;
    }
  }
}

auto EncoderInput::scroll_sensitivity(uint8_t val) -> void {
  scroll_sensitivity_ = val;
  relative_wheel_->SetSensitivity(scroll_sensitivity_);
}

auto EncoderInput::UpdateKeyState(Keys key, uint64_t ms, bool clicked) -> void {
  if (clicked) {
    if (!touch_time_ms_.contains(key)) {
      // Key was just clicked.
      touch_time_ms_[key] = ms;
      just_released_.erase(key);
      fired_.erase(key);
    }
    return;
  }

  // Key is not clicked.
  if (touch_time_ms_.contains(key)) {
    // Key was just released.
    just_released_.insert(key);
    touch_time_ms_.erase(key);
  }
}

auto EncoderInput::TriggerKey(Keys key, KeyStyle s, uint64_t ms) -> Trigger {
  if (s == KeyStyle::kRepeat) {
    bool may_repeat = fired_.contains(key) && touch_time_ms_.contains(key) &&
                      ms - touch_time_ms_[key] >= kRepeatDelayMs;

    // Repeatable keys trigger on press.
    if (touch_time_ms_.contains(key) && (!fired_.contains(key) || may_repeat)) {
      fired_.insert(key);
      return Trigger::kClick;
    } else {
      return Trigger::kNone;
    }
  } else if (s == KeyStyle::kLongPress) {
    // Long press keys trigger on release, or after holding for a delay.
    if (just_released_.contains(key)) {
      just_released_.erase(key);
      if (!fired_.contains(key)) {
        fired_.insert(key);
        return Trigger::kClick;
      }
    }
    if (touch_time_ms_.contains(key) &&
        ms - touch_time_ms_[key] >= kLongPressDelayMs &&
        !fired_.contains(key)) {
      fired_.insert(key);
      return Trigger::kLongPress;
    }
  }

  return Trigger::kNone;
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
