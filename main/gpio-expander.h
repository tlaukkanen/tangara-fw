#pragma once

#include <stdint.h>

#include "esp_check.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define PCA8575_ADDRESS (0x20)
#define PCA8575_TIMEOUT (5)

namespace gay_ipod {

/**
 * PCA8575
 */
class GpioExpander {
  public:
    GpioExpander();
    ~GpioExpander();

    esp_err_t Write(void);
    esp_err_t Read(void);

    bool charge_power_ok(void) const;
    bool headphone_detect(void) const;
    uint8_t key_states(void) const;

    enum SdMuxController {
      ESP,
      USB
    };

    void set_sd_mux(SdMuxController controller);
    void set_sd_cs(bool high);
    void set_display_cs(bool high);
    
    // Not copyable or movable.
    // TODO: maybe this could be movable?
    GpioExpander(const GpioExpander&) = delete;
    GpioExpander& operator=(const GpioExpander&) = delete;

  private:
    // Port A:
    // 0 - audio power enable
    // 1 - usb interface power enable
    // 2 - display power enable
    // 3 - sd card power enable
    // 4 - charge power ok (active low)
    // 5 - sd mux switch
    // 6 - sd chip select
    // 7 - display chip select
    // All power switches low, chip selects high, active-low charge power high
    uint8_t port_a_ = 0b11010000;

    // Port B:
    // 0 - 3.5mm jack detect (active low)
    // 1 - dac soft mute switch
    // 2 - GPIO
    // 3 - GPIO
    // 4 - GPIO
    // 5 - GPIO
    // 6 - GPIO
    // 7 - GPIO
    // DAC mute output low, everything else is active-low inputs.
    uint8_t port_b_ = 0b11111101;

    uint8_t input_a_ = 0;
    uint8_t input_b_ = 0;
};

} // namespace gay_ipod
