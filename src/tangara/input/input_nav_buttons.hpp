/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "drivers/gpios.hpp"
#include "hal/lv_hal_indev.h"

#include "drivers/haptics.hpp"
#include "input/input_device.hpp"
#include "input/input_hook.hpp"
#include "input/input_trigger.hpp"
#include "drivers/touchwheel.hpp"

namespace input {

class NavButtons : public IInputDevice {
 public:
  NavButtons(drivers::IGpios&);

  auto read(lv_indev_data_t* data) -> void override;

  auto name() -> std::string override;
  auto triggers() -> std::vector<std::reference_wrapper<TriggerHooks>> override;

 private:
  drivers::IGpios& gpios_;

  TriggerHooks up_;
  TriggerHooks down_;
};

}  // namespace input