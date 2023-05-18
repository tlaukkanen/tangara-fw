#include "battery.hpp"
#include <cstdint>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"

namespace drivers {

static const adc_bitwidth_t kAdcBitWidth = ADC_BITWIDTH_12;
static const adc_unit_t kAdcUnit = ADC_UNIT_1;
// Max battery voltage should be a little over 2V due to our divider, so we need
// the max attenuation to properly handle the full range.
static const adc_atten_t kAdcAttenuation = ADC_ATTEN_DB_11;
// Corresponds to SENSOR_VP.
static const adc_channel_t kAdcChannel = ADC_CHANNEL_0;

Battery::Battery() {
  adc_oneshot_unit_init_cfg_t unit_config = {
      .unit_id = kAdcUnit,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle_));

  adc_oneshot_chan_cfg_t channel_config = {
      .atten = kAdcAttenuation,
      .bitwidth = kAdcBitWidth,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc_handle_, kAdcChannel, &channel_config));

  // calibrate
  // TODO: compile-time assert our scheme is available
  adc_cali_line_fitting_config_t calibration_config = {
      .unit_id = kAdcUnit,
      .atten = kAdcAttenuation,
      .bitwidth = kAdcBitWidth,
  };
  ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(
      &calibration_config, &adc_calibration_handle_));
}

Battery::~Battery() {
  adc_cali_delete_scheme_line_fitting(adc_calibration_handle_);
  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle_));
}

auto Battery::Millivolts() -> uint32_t {
  // GPIO 34
  int raw = 0;
  ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, kAdcChannel, &raw));

  int voltage = 0;
  ESP_ERROR_CHECK(
      adc_cali_raw_to_voltage(adc_calibration_handle_, raw, &voltage));

  return voltage;
}

}  // namespace drivers
