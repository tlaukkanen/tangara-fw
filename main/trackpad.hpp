#pragma once

#include "gpio-expander.hpp"

#include <functional>
#include <stdint.h>

#include "esp_err.h"
#include "result.hpp"

namespace gay_ipod {

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

  int GetZLevel();

 private:
  GpioExpander* gpio_;
  
  enum Register {
    STATUS1 = 0x02, // Contains status flags about the state of Pinnacle
    Z_LEVEL = 0x17, 
  };

  void ClearFlags();

  void WriteRegister(uint8_t reg, uint8_t val);
  void ReadRegister(uint8_t reg, uint8_t* data, uint8_t count);

};

}  // namespace gay_ipod
