/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "battery.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

namespace drivers {

TEST_CASE("battery measurement", "[integration]") {
  Battery battery;

  SECTION("voltage is within range") {
    uint32_t mv = battery.Millivolts();
    REQUIRE(mv <= 2200);  // Plugged in, no battery.
    REQUIRE(mv >= 1000);
  }
}

}  // namespace drivers
