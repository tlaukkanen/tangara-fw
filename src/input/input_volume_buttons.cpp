/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_volume_buttons.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"

namespace input {

VolumeButtons::VolumeButtons(drivers::IGpios& gpios) : gpios_(gpios) {}

auto VolumeButtons::read(lv_indev_data_t* data) -> void {
  bool vol_up = gpios_.Get(drivers::IGpios::Pin::kKeyUp);
  switch (up_.update(!vol_up)) {
    case Trigger::State::kNone:
      break;
    default:
      events::Audio().Dispatch(audio::StepUpVolume{});
      break;
  }

  bool vol_down = gpios_.Get(drivers::IGpios::Pin::kKeyDown);
  switch (down_.update(!vol_down)) {
    case Trigger::State::kNone:
      break;
    default:
      events::Audio().Dispatch(audio::StepDownVolume{});
      break;
  }
}

}  // namespace input
