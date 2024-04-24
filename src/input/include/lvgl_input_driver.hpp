/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <set>

#include "core/lv_group.h"
#include "device_factory.hpp"
#include "feedback_device.hpp"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"

#include "input_device.hpp"
#include "nvs.hpp"
#include "property.hpp"
#include "touchwheel.hpp"

namespace input {

/*
 * Implementation of an LVGL input device. This class composes multiple
 * IInputDevice and IFeedbackDevice instances together into a single LVGL
 * device.
 */
class LvglInputDriver {
 public:
  LvglInputDriver(drivers::NvsStorage& nvs, DeviceFactory&);

  auto mode() -> lua::Property& { return mode_; }

  auto read(lv_indev_data_t* data) -> void;
  auto feedback(uint8_t) -> void;

  auto registration() -> lv_indev_t* { return registration_; }
  auto lock(bool l) -> void { is_locked_ = l; }

  auto pushHooks(lua_State* L) -> int;

 private:
  drivers::NvsStorage& nvs_;
  DeviceFactory& factory_;

  lua::Property mode_;
  lv_indev_drv_t driver_;
  lv_indev_t* registration_;

  std::vector<std::shared_ptr<IInputDevice>> inputs_;
  std::vector<std::shared_ptr<IFeedbackDevice>> feedbacks_;

  bool is_locked_;
};

}  // namespace input
