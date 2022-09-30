#ifndef BATTERY_H
#define BATTERY_H

namespace gay_ipod {

esp_error_t init_adc(void);

/**
 * Returns the current battery level in millivolts.
 */
uint32_t read_battery_voltage(void);

} // namespace gay_ipod

#endif
