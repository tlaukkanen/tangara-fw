/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

namespace input {

/*
 * Interface for providing non-visual feedback to the user as a result of LVGL
 * events. 'Feedback Devices' are able to observe all events that are generated
 * by LVGL as a result of Input Devices.
 *
 * Implementations of this interface are a mix of hardware features (e.g. a
 * haptic motor buzzing when your selection changes) and firmware features
 * (e.g. playing audio feedback that describes the selected element).
 */
class IFeedbackDevice {
 public:
  virtual ~IFeedbackDevice() {}

  virtual auto feedback(uint8_t event_type) -> void = 0;

  // TODO: Add configuration; likely the same shape of interface that
  // IInputDevice uses.
};

}  // namespace input
