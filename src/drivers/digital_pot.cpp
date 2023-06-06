/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "digital_pot.hpp"

#include <cstdint>

namespace drivers {

using GpioExpander::VOL_LEFT;
using GpioExpander::VOL_RIGHT;
using GpioExpander::VOL_UP_DOWN;
using GpioExpander::VOL_Z_CROSS;

DigitalPot::DigitalPot(GpioExpander* gpios) : gpios_(gpios) {
  gpios_->set_pin(VOL_Z_CROSS, true);  // Active-low
  gpios_->set_pin(VOL_UP_DOWN, true);
  gpios_->set_pin(VOL_LEFT, false);
  gpios_->set_pin(VOL_RIGHT, false);
  gpios_->Write();

  // Power-on reset sets attenuation to maximum anyway, but we want to be safe
  // and not blow anyone's ears out.
  for (int i = 0; i < 32; i++) {
    gpios_->set_pin(VOL_LEFT, true);
    gpios_->set_pin(VOL_RIGHT, true);
    gpios_->Write();
    gpios_->set_pin(VOL_LEFT, false);
    gpios_->set_pin(VOL_RIGHT, false);
    gpios_->Write();
  }
}

auto DigitalPot::SetRelative(int_fast8_t change) -> void {
  if (change == 0) {
    return;
  }

  gpios_->set_pin(VOL_UP_DOWN, change > 0);
  gpios_->Write();

  for (int i = 0; i < std::abs(change); i++) {
    gpios_->set_pin(VOL_LEFT, true);
    gpios_->set_pin(VOL_RIGHT, true);
    gpios_->Write();
    gpios_->set_pin(VOL_LEFT, false);
    gpios_->set_pin(VOL_RIGHT, false);
    gpios_->Write();
  }
}

auto DigitalPot::SetRelative(Channel ch, int_fast8_t change) -> void {
  if (change == 0) {
    return;
  }

  GpioExpander::Pin pin = (ch == Channel::kLeft) ? VOL_LEFT : VOL_RIGHT;
  gpios_->set_pin(VOL_UP_DOWN, change > 0);
  gpios_->Write();

  for (int i = 0; i < std::abs(change); i++) {
    gpios_->set_pin(pin, true);
    gpios_->Write();
    gpios_->set_pin(pin, false);
    gpios_->Write();
  }
}

auto DigitalPot::SetZeroCrossDetect(bool enabled) -> void {
  gpios_->set_pin(VOL_Z_CROSS, !enabled);  // Active-low
  gpios_->Write();
}

auto DigitalPot::GetMaxAttenuation() -> int_fast8_t {
  return 31;
}

auto DigitalPot::GetMinAttenuation() -> int_fast8_t {
  return 0;
}

}  // namespace drivers
