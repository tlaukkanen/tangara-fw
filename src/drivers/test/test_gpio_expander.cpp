#include "gpio_expander.hpp"

#include "catch2/catch.hpp"

#include "i2c.hpp"
#include "i2c_fixture.hpp"

namespace drivers {

TEST_CASE("gpio expander", "[integration]") {
  I2CFixture i2c;
  GpioExpander expander;
  SECTION("with() writes when ") {
    // Initial value.
    expander.Read();
    REQUIRE(expander.get_input(GpioExpander::KEY_DOWN) == true);

    expander.with(
        [&](auto& gpio) { gpio.set_pin(GpioExpander::KEY_DOWN, false); });

    expander.Read();
    REQUIRE(expander.get_input(GpioExpander::KEY_DOWN) == false);
  }
}

}  // namespace drivers
