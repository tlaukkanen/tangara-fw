/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "encoder_input.hpp"

#include <sys/_stdint.h>
#include <memory>

#include "core/lv_group.h"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"
#include "relative_wheel.hpp"
#include "touchwheel.hpp"

namespace ui {

static void encoder_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  EncoderInput* instance = reinterpret_cast<EncoderInput*>(drv->user_data);
  instance->Read(data);
}

EncoderInput::EncoderInput(drivers::IGpios& gpios, drivers::TouchWheel& wheel)
    : gpios_(gpios),
      raw_wheel_(wheel),
      relative_wheel_(std::make_unique<drivers::RelativeWheel>(wheel)) {
  lv_indev_drv_init(&driver_);
  driver_.type = LV_INDEV_TYPE_ENCODER;
  driver_.read_cb = encoder_read;
  driver_.user_data = this;

  registration_ = lv_indev_drv_register(&driver_);
}

auto EncoderInput::Read(lv_indev_data_t* data) -> void {
  raw_wheel_.Update();
  relative_wheel_->Update();

  data->enc_diff = relative_wheel_->ticks();
  data->state = relative_wheel_->is_clicking() ? LV_INDEV_STATE_PRESSED
                                               : LV_INDEV_STATE_RELEASED;
}

}  // namespace ui
