/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#include "drivers/wm8523.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

#include "drivers/gpios.hpp"
#include "drivers/i2c.hpp"
#include "i2c_fixture.hpp"

namespace drivers {

TEST_CASE("dac is present", "[integration]") {
  I2CFixture i2c;

  SECTION("device id is correct") {
    auto res = wm8523::ReadRegister(wm8523::Register::kReset);

    REQUIRE(res.has_value());
    REQUIRE(res.value() == 0x8523);
  }
}

}  // namespace drivers
