#include "battery.hpp"
#include <cstdint>

#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"

namespace drivers {

static const uint8_t kAdcBitWidth = ADC_BITWIDTH_12;
static const uint8_t kAdcUnit = ADC_UNIT_1;
// Max battery voltage should be a little over 2V due to our divider, so we need
// the max attenuation to properly handle the full range.
static const uint8_t kAdcAttenuation = ADC_ATTEN_DB_11;
// Corresponds to GPIO 34.
static const uint8_t kAdcChannel = ADC_CHANNEL_6;

Battery::Battery() {
  adc_oneshot_unit_init_cfg_t unit_config = {
    .unit_id = kAdcUnit,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle_));

  adc_oneshot_chan_cfg_t channel_config = {
    .bitwidth = kAdcBitWidth,
    .atten = kAdcAttenuation,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, kAdcChannel, &channel_config));
  
  // calibrate
  // TODO: compile-time assert our scheme is available
  adc_cali_line_fitting_config_t calibration_config = {
    .unit_id = kAdcUnit,
    .atten = kAdcAttenuation,
    .bitwidth = kAdcBitWidth,
  };
  ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&calibration_config, &adc_calibration_handle_));
}

Battery::~Battery() {
  adc_cali_delete_scheme_line_fitting(adc_calibration_handle_);
}

auto Battery::Millivolts() -> uint32_t {
  // GPIO 34
  int raw;
  ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, kAdcChannel &raw));

  int voltage;
  ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_calibration_handle, raw, &voltage));

  return voltage;
}

}  // namespace drivers
