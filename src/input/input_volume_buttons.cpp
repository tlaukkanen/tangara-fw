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
    : gpios_(gpios), up_(actions::volumeUp), down_(actions::volumeDown) {}

auto VolumeButtons::read(lv_indev_data_t* data) -> void {
  up_.update(!gpios_.Get(drivers::IGpios::Pin::kKeyUp), data);
  down_.update(!gpios_.Get(drivers::IGpios::Pin::kKeyDown), data);
}

}  // namespace input
