/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dac.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

#include "gpio_expander.hpp"
#include "i2c.hpp"
#include "i2c_fixture.hpp"

namespace drivers {

TEST_CASE("dac configuration", "[integration]") {
  I2CFixture i2c;
  GpioExpander expander;
  cpp::result<AudioDac*, AudioDac::Error> dac_res = AudioDac::create(&expander);
  REQUIRE(dac_res.has_value());
  std::unique_ptr<AudioDac> dac(dac_res.value());

  auto power_state = dac->ReadPowerState();

  REQUIRE(power_state.first == true);  // booted
}

}  // namespace drivers
