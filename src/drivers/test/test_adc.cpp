/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/adc.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

namespace drivers {

TEST_CASE("battery adc", "[integration]") {
  AdcBattery battery;

  SECTION("voltage is within range") {
    uint32_t mv = battery.Millivolts();
    REQUIRE(mv <= 4210); // Should never be charged above this.
    REQUIRE(mv >= 3000); // Should never be discharged below this.
  }
}

}  // namespace drivers
