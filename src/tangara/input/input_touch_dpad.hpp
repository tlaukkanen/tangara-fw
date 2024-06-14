/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "indev/lv_indev.h"

#include "drivers/haptics.hpp"
#include "input/input_device.hpp"
#include "input/input_hook.hpp"
#include "input/input_trigger.hpp"
#include "drivers/touchwheel.hpp"

namespace input {

class TouchDPad : public IInputDevice {
 public:
  TouchDPad(drivers::TouchWheel&);

  auto read(lv_indev_data_t* data) -> void override;

  auto name() -> std::string override;
  auto triggers() -> std::vector<std::reference_wrapper<TriggerHooks>> override;

 private:
  drivers::TouchWheel& wheel_;

  TriggerHooks centre_;
  TriggerHooks up_;
  TriggerHooks right_;
  TriggerHooks down_;
  TriggerHooks left_;
};

}  // namespace input
