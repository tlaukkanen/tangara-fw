#include "battery.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "hal/adc_types.h"

namespace gay_ipod {

static esp_adc_cal_characteristics_t calibration;

esp_err_t init_adc(void) {
  // Calibration should already be fused into the chip from the factory, so
  // we should only need to read it back out again.
  esp_adc_cal_characterize(
      ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, &calibration);

  // Max battery voltage should be a little over 2V due to our divider, so
  // we need the max attenuation to properly handle the full range.
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

  return ESP_OK;
}

uint32_t read_battery_voltage(void) {
  // GPIO 34
  int raw = adc1_get_raw(ADC1_CHANNEL_6);
  return esp_adc_cal_raw_to_voltage(raw, &calibration);
}

} // namespace gay_ipod
