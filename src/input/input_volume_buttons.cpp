/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_volume_buttons.hpp"
#include "gpios.hpp"

namespace input {

VolumeButtons::VolumeButtons(drivers::IGpios& gpios) : gpios_(gpios) {}

auto VolumeButtons::read(lv_indev_data_t* data) -> void {
  bool vol_up = gpios_.Get(drivers::IGpios::Pin::kKeyUp);
  if (!vol_up) {
    ESP_LOGI("volume", "vol up");
  }

  bool vol_down = gpios_.Get(drivers::IGpios::Pin::kKeyDown);
  if (!vol_down) {
    ESP_LOGI("volume", "vol down");
  }
}

}  // namespace input
