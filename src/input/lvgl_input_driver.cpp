/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lvgl_input_driver.hpp"
#include <stdint.h>

#include <cstdint>
#include <memory>

#include "feedback_haptics.hpp"
#include "input_touch_wheel.hpp"
#include "input_volume_buttons.hpp"
#include "lvgl.h"
#include "service_locator.hpp"

[[maybe_unused]] static constexpr char kTag[] = "input";

namespace input {

static void read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(drv->user_data);
  instance->read(data);
}

static void feedback_cb(lv_indev_drv_t* drv, uint8_t event) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(drv->user_data);
  instance->feedback(event);
}

LvglInputDriver::LvglInputDriver(
    std::shared_ptr<system_fsm::ServiceLocator> services)
    : services_(services),
      driver_(),
      registration_(nullptr),
      inputs_(),
      feedbacks_(),
      is_locked_(false) {
  lv_indev_drv_init(&driver_);
  driver_.type = LV_INDEV_TYPE_ENCODER;
  driver_.read_cb = read_cb;
  driver_.feedback_cb = feedback_cb;
  driver_.user_data = this;

  registration_ = lv_indev_drv_register(&driver_);

  // TODO: Make these devices configurable. I'm thinking each device gets an id
  // and then we have:
  //  - a factory to create instance given an id
  //  - add/remove device methods on LvglInputDriver that operate on ids
  //  - the user's enabled devices (+ their configuration) stored in NVS.
  auto touchwheel = services_->touchwheel();
  if (touchwheel) {
    inputs_.push_back(std::make_unique<TouchWheel>(**touchwheel));
  }
  inputs_.push_back(std::make_unique<VolumeButtons>(services_->gpios()));
  feedbacks_.push_back(std::make_unique<Haptics>(services_->haptics()));
}

auto LvglInputDriver::read(lv_indev_data_t* data) -> void {
  // TODO: we should pass lock state on to the individual devices, since they
  // may wish to either ignore the lock state, or power down until unlock.
  if (is_locked_) {
    return;
  }
  for (auto&& device : inputs_) {
    device->read(data);
  }
}

auto LvglInputDriver::feedback(uint8_t event) -> void {
  if (is_locked_) {
    return;
  }
  for (auto&& device : feedbacks_) {
    device->feedback(event);
  }
}

}  // namespace input
