#ifndef GPIO_EXPANDER_H
#define GPIO_EXPANDER_H

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

    bool charge_power_ok(void);
    bool headphone_detect(void);
    uint8_t key_states(void);

    // Not copyable or movable.
    // TODO: maybe this could be movable?
    GpioExpander(const GpioExpander&) = delete;
    GpioExpander& operator=(const GpioExpander&) = delete;

  private:
    // All power switches high, PWR_OK low (input), both CS pins high.
    uint8_t port_a_ = uint8_t{0b00001000};
    // DAC mute output low, everything else is input and so low.
    uint8_t port_b_ = uint8_t{0b11111111};

    uint8_t input_a_ = 0;
    uint8_t input_b_ = 0;
};

} // namespace gay_ipod

#endif
