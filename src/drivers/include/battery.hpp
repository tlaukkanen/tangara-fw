/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "result.hpp"

namespace drivers {

class Battery {
 public:
  static auto Create() -> Battery* { return new Battery(); }
  Battery();
  ~Battery();

  /**
   * Returns the current battery level in millivolts.
   */
  auto Millivolts() -> uint32_t;

  auto UpdatePercent() -> bool;
  auto Percent() -> uint_fast8_t { return percent_; }

 private:
  adc_oneshot_unit_handle_t adc_handle_;
  adc_cali_handle_t adc_calibration_handle_;

  uint_fast8_t percent_;
};

}  // namespace drivers
