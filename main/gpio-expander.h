#ifndef GPIO_EXPANDER_H
#define GPIO_EXPANDER_H

#include <stdint.h>

#include "esp_check.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define PCA8575_ADDRESS (0x20)

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

    bool charge_power_ok(void);
    bool headphone_detect(void);
    uint8_t key_states(void);

    // Not copyable or movable.
    // TODO: maybe this could be movable?
    GpioExpander(const GpioExpander&) = delete;
    GpioExpander& operator=(const GpioExpander&) = delete;

  private:
    // All power switches low, both CS pins high, active-low PWR_OK high.
    uint8_t port_a_ = 0b00001011;
    // DAC mute output low, everything eelse is input and so held high.
    uint8_t port_b_ = 0b10111111;

    uint8_t input_a_ = 0;
    uint8_t input_b_ = 0;
};

} // namespace gay_ipod

#endif
