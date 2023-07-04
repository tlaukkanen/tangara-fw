/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "wheel_encoder.hpp"
#include <sys/_stdint.h>
#include "core/lv_group.h"
#include "hal/lv_hal_indev.h"

namespace ui {

void encoder_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  TouchWheelEncoder* instance =
      reinterpret_cast<TouchWheelEncoder*>(drv->user_data);
  ESP_LOGI("Wheel Encoder", "Before read state: %d", data->state);
  instance->Read(data);
  ESP_LOGI("Wheel Encoder", "After read state: %d", data->state);
}

void encoder_feedback(lv_indev_drv_t* drv, uint8_t event_code) {
  ESP_LOGI("Touchwheel Event", "Event code: %d", event_code);
}

TouchWheelEncoder::TouchWheelEncoder(
    std::weak_ptr<drivers::RelativeWheel> wheel)
    : last_key_(0), wheel_(wheel) {
  lv_indev_drv_init(&driver_);
  driver_.type = LV_INDEV_TYPE_KEYPAD;
  driver_.read_cb = encoder_read;
  driver_.feedback_cb = encoder_feedback;
  driver_.user_data = this;

  registration_ = lv_indev_drv_register(&driver_);
}

auto TouchWheelEncoder::Read(lv_indev_data_t* data) -> void {
  auto lock = wheel_.lock();
  if (lock == nullptr) {
    return;
  }

  lock->Update();


  auto ticks = lock->ticks();
  if (ticks > 0) {
    ESP_LOGI("wheel encoder", "is prev");
    data->key = LV_KEY_PREV;
    data->state = LV_INDEV_STATE_PRESSED;
  } else if (ticks < 0) {
    ESP_LOGI("wheel encoder", "is next");
    data->key = LV_KEY_NEXT;
    data->state = LV_INDEV_STATE_PRESSED;
  } else if (lock->is_clicking()) {
    ESP_LOGI("wheel encoder", "is clicking");
    data->key = LV_KEY_ENTER;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

}  // namespace ui
