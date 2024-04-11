/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_nav_buttons.hpp"

#include "event_queue.hpp"
#include "gpios.hpp"
#include "hal/lv_hal_indev.h"

namespace input {

NavButtons::NavButtons(drivers::IGpios& gpios) : gpios_(gpios) {}

auto NavButtons::read(lv_indev_data_t* data) -> void {
  bool vol_up = gpios_.Get(drivers::IGpios::Pin::kKeyUp);
  switch (up_.update(!vol_up)) {
    case Trigger::State::kClick:
      data->enc_diff = -1;
      break;
    case Trigger::State::kLongPress:
      events::Ui().Dispatch(ui::internal::BackPressed{});
      break;
    default:
      break;
  }

  bool vol_down = gpios_.Get(drivers::IGpios::Pin::kKeyDown);
  switch (down_.update(!vol_down)) {
    case Trigger::State::kClick:
      data->enc_diff = 1;
      break;
    case Trigger::State::kLongPress:
      data->state = LV_INDEV_STATE_PRESSED;
      break;
    default:
      data->state = LV_INDEV_STATE_RELEASED;
      break;
  }
}

}  // namespace input
