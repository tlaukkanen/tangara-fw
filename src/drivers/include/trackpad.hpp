#pragma once


#include <functional>
#include <stdint.h>

#include "esp_err.h"
#include "result.hpp"

#include "gpio_expander.hpp"

namespace drivers {

struct TrackpadData {
  bool is_touched;
  uint16_t x_position;
  uint16_t y_position;
  uint16_t z_level;
};

/**
 * Interface for a PCM5122PWR DAC, configured over I2C.
 */
class Trackpad {
 public:
  enum Error {
    FAILED_TO_BOOT,
    FAILED_TO_CONFIGURE,
  };
  static auto create(GpioExpander* expander)
      -> cpp::result<std::unique_ptr<Trackpad>, Error>;

  Trackpad(GpioExpander* gpio);
  ~Trackpad();

  // Not copyable or movable.
  Trackpad(const Trackpad&) = delete;
  Trackpad& operator=(const Trackpad&) = delete;

  auto Update() -> void;
  auto GetTrackpadData() const -> TrackpadData;

 private:
  GpioExpander* gpio_;
  TrackpadData trackpad_data_;
  
  enum Register {
    STATUS1 = 0x02, // Contains status flags about the state of Pinnacle
    X_LOW_BITS = 0x14, 
    Y_LOW_BITS = 0x15, 
    X_Y_HIGH_BITS = 0x16,
    Z_LEVEL = 0x17, 
  };

  void ClearFlags();

  void WriteRegister(uint8_t reg, uint8_t val);
  void ReadRegister(uint8_t reg, uint8_t* data, uint8_t count);

};

}  // namespace drivers
