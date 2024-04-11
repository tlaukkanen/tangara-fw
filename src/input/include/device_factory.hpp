/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>

#include "feedback_device.hpp"
#include "input_device.hpp"
#include "input_touch_wheel.hpp"
#include "nvs.hpp"
#include "service_locator.hpp"

namespace input {

class DeviceFactory {
 public:
  DeviceFactory(std::shared_ptr<system_fsm::ServiceLocator>);

  auto createInputs(drivers::NvsStorage::InputModes mode)
      -> std::vector<std::shared_ptr<IInputDevice>>;

  auto createFeedbacks() -> std::vector<std::shared_ptr<IFeedbackDevice>>;

  auto touch_wheel() -> std::shared_ptr<TouchWheel> { return wheel_; }

 private:
  std::shared_ptr<system_fsm::ServiceLocator> services_;

  // HACK: the touchwheel is current a special case, since it's the only input
  // device that has some kind of setting/configuration; scroll sensitivity.
  std::shared_ptr<TouchWheel> wheel_;
};

}  // namespace input
