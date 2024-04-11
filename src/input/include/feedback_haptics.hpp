/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "feedback_device.hpp"
#include "haptics.hpp"

namespace input {

class Haptics : public IFeedbackDevice {
 public:
  Haptics(drivers::Haptics& haptics_);

  auto feedback(uint8_t event_type) -> void override;

 private:
  drivers::Haptics& haptics_;
};

}  // namespace input
