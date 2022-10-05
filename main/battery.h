#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace gay_ipod {

esp_err_t init_adc(void);

/**
 * Returns the current battery level in millivolts.
 */
uint32_t read_battery_voltage(void);

} // namespace gay_ipod
