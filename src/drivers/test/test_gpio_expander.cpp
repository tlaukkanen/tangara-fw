/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gpios.hpp"

#include "catch2/catch.hpp"

#include "i2c.hpp"
#include "i2c_fixture.hpp"

namespace drivers {

TEST_CASE("gpio expander", "[integration]") {
  I2CFixture i2c;
  IGpios expander;
  SECTION("with() writes when ") {
    // Initial value.
    expander.Read();
    REQUIRE(expander.get_input(IGpios::KEY_DOWN) == true);

    expander.with([&](auto& gpio) { gpio.set_pin(IGpios::KEY_DOWN, false); });

    expander.Read();
    REQUIRE(expander.get_input(IGpios::KEY_DOWN) == false);
  }
}

}  // namespace drivers
