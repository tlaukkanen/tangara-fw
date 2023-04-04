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
    FIRMWARE_VERSION = 0x1,
    DETECTION_STATUS = 0x2,
    KEY_STATUS_A = 0x3,
    KEY_STATUS_B = 0x4,
    SLIDER_POSITION = 0x5,
    CALIBRATE = 0x6,
    RESET = 0x7,
    LOW_POWER = 0x8,
    SLIDER_OPTIONS = 0x14,
  };

  void WriteRegister(uint8_t reg, uint8_t val);
  uint8_t ReadRegister(uint8_t reg);
};

}  // namespace drivers
