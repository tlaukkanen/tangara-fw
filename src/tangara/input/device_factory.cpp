/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/device_factory.hpp"

#include <memory>

#include "input/feedback_haptics.hpp"
#include "input/input_device.hpp"
#include "input/input_nav_buttons.hpp"
#include "input/input_touch_dpad.hpp"
#include "input/input_touch_wheel.hpp"
#include "input/input_volume_buttons.hpp"

namespace input {

DeviceFactory::DeviceFactory(
    std::shared_ptr<system_fsm::ServiceLocator> services)
    : services_(services) {
  if (services->touchwheel()) {
    wheel_ =
        std::make_shared<TouchWheel>(services->nvs(), **services->touchwheel());
  }
}

auto DeviceFactory::createInputs(drivers::NvsStorage::InputModes mode)
    -> std::vector<std::shared_ptr<IInputDevice>> {
  std::vector<std::shared_ptr<IInputDevice>> ret;
  switch (mode) {
    case drivers::NvsStorage::InputModes::kButtonsOnly:
      ret.push_back(std::make_shared<NavButtons>(services_->gpios()));
      break;
    case drivers::NvsStorage::InputModes::kDirectionalWheel:
      ret.push_back(std::make_shared<VolumeButtons>(services_->gpios()));
      if (services_->touchwheel()) {
        ret.push_back(std::make_shared<TouchDPad>(**services_->touchwheel()));
      }
      break;
    case drivers::NvsStorage::InputModes::kRotatingWheel:
    default:  // Don't break input over a bad enum value.
      ret.push_back(std::make_shared<VolumeButtons>(services_->gpios()));
      if (wheel_) {
        ret.push_back(wheel_);
      }
      break;
  }
  return ret;
}

auto DeviceFactory::createFeedbacks()
    -> std::vector<std::shared_ptr<IFeedbackDevice>> {
  return {std::make_shared<Haptics>(services_->haptics())};
}

}  // namespace input
