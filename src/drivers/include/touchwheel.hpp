/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <functional>

#include "esp_err.h"
#include "result.hpp"

#include "gpios.hpp"

namespace drivers {

struct TouchWheelData {
  bool is_wheel_touched = false;
  bool is_button_touched = false;
  uint8_t wheel_position = -1;
};

class TouchWheel {
 public:
  static auto isAngleWithin(int16_t wheel_angle,
                            int16_t target_angle,
                            int threshold) -> bool;

  static auto Create() -> TouchWheel* { return new TouchWheel(); }
  TouchWheel();
  ~TouchWheel();

  // Not copyable or movable.
  TouchWheel(const TouchWheel&) = delete;
  TouchWheel& operator=(const TouchWheel&) = delete;

  auto Update() -> void;
  auto GetTouchWheelData() const -> TouchWheelData;

  auto PowerDown() -> void;

 private:
  TouchWheelData data_;

  enum Register {
    FIRMWARE_VERSION = 1,
    DETECTION_STATUS = 2,
    KEY_STATUS_A = 3,
    KEY_STATUS_B = 4,
    SLIDER_POSITION = 5,
    CALIBRATE = 6,
    RESET = 7,
    LOW_POWER = 8,
    RECALIBRATION_DELAY = 12,
    SLIDER_OPTIONS = 14,
    CHARGE_TIME = 15,
    DETECT_THRESHOLD_BASE = 16,
    KEY_CONTROL_BASE = 28,
    PULSE_SCALE_BASE = 40,
  };

  void WriteRegister(uint8_t reg, uint8_t val);
  uint8_t ReadRegister(uint8_t reg);
};

}  // namespace drivers
