/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "wheel_encoder.hpp"
#include "hal/lv_hal_indev.h"

namespace ui {

void encoder_read(lv_indev_drv_t * drv, lv_indev_data_t*data){
  TouchWheelEncoder *instance = reinterpret_cast<TouchWheelEncoder*>(drv->user_data);
  instance->Read(data);
}

  TouchWheelEncoder::TouchWheelEncoder(std::weak_ptr<drivers::RelativeWheel> wheel) : wheel_(wheel) {
    lv_indev_drv_init(&driver_);
    driver_.type = LV_INDEV_TYPE_ENCODER;
    driver_.read_cb = encoder_read;
    driver_.user_data = this;

    registration_ = lv_indev_drv_register(&driver_);
  }

auto TouchWheelEncoder::Read(lv_indev_data_t *data) -> void {
  auto lock = wheel_.lock();
  if (lock == nullptr) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->enc_diff = 0;
    return;
  }

  lock->Update();
  data->state = lock->is_pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->enc_diff = lock->ticks();
}

} // namespace ui
