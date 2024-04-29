/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_volume_buttons.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "input_hook_actions.hpp"

namespace input {

VolumeButtons::VolumeButtons(drivers::IGpios& gpios)
    : gpios_(gpios),
      up_("upper", actions::volumeUp()),
      down_("lower", actions::volumeDown()) {}

auto VolumeButtons::read(lv_indev_data_t* data) -> void {
  up_.update(!gpios_.Get(drivers::IGpios::Pin::kKeyUp), data);
  down_.update(!gpios_.Get(drivers::IGpios::Pin::kKeyDown), data);
}

auto VolumeButtons::name() -> std::string {
  return "buttons";
}

auto VolumeButtons::triggers()
    -> std::vector<std::reference_wrapper<TriggerHooks>> {
  return {up_, down_};
}

}  // namespace input
