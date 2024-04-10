/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "hal/lv_hal_indev.h"

namespace input {

/*
 * Interface for all device input methods. Each 'Input Device' is polled by
 * LVGL at regular intervals, and can effect the device either via LVGL's input
 * device driver API, or by emitting events for other parts of the system to
 * react to (e.g. issuing a play/pause event, or altering the volume).
 */
class IInputDevice {
 public:
  virtual ~IInputDevice() {}

  virtual auto read(lv_indev_data_t* data) -> void = 0;

  // TODO: Add hooks and configuration (or are hooks just one kind of
  // configuration?)
};

}  // namespace input
