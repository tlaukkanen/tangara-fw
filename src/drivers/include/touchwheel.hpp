#pragma once

#include <stdint.h>
#include <functional>

#include "esp_err.h"
#include "result.hpp"

#include "gpio_expander.hpp"

namespace drivers {

struct TouchWheelData {
  bool is_touched = false;
  uint8_t wheel_position = -1;
};

class TouchWheel {
 public:
  static auto Create() -> TouchWheel* { return new TouchWheel(); }
  TouchWheel();
  ~TouchWheel();

  // Not copyable or movable.
  TouchWheel(const TouchWheel&) = delete;
  TouchWheel& operator=(const TouchWheel&) = delete;

  auto Update() -> void;
  auto GetTouchWheelData() const -> TouchWheelData;

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
    SLIDER_OPTIONS = 14,
    CHARGE_TIME = 15,
    KEY_CONTROL_BASE = 28,
  };

  void WriteRegister(uint8_t reg, uint8_t val);
  uint8_t ReadRegister(uint8_t reg);
};

}  // namespace drivers
