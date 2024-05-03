/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/input_nav_buttons.hpp"

#include "drivers/gpios.hpp"
#include "events/event_queue.hpp"
#include "hal/lv_hal_indev.h"
#include "input/input_hook_actions.hpp"

namespace input {

NavButtons::NavButtons(drivers::IGpios& gpios)
    : gpios_(gpios),
      up_("upper", {}, actions::scrollUp(), actions::select(), {}),
      down_("lower", {}, actions::scrollDown(), actions::select(), {}) {}

auto NavButtons::read(lv_indev_data_t* data) -> void {
  up_.update(!gpios_.Get(drivers::IGpios::Pin::kKeyUp), data);
  down_.update(!gpios_.Get(drivers::IGpios::Pin::kKeyDown), data);
}

auto NavButtons::name() -> std::string {
  return "buttons";
}

auto NavButtons::triggers()
    -> std::vector<std::reference_wrapper<TriggerHooks>> {
  return {up_, down_};
}

}  // namespace input
