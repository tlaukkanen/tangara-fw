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
    REQUIRE(expander.get_input(GpioExpander::GPIO_1) == true);

    expander.with(
        [&](auto& gpio) { gpio.set_pin(GpioExpander::GPIO_1, false); });

    expander.Read();
    REQUIRE(expander.get_input(GpioExpander::GPIO_1) == false);
  }

  SECTION("setting individual pins") {
    expander.set_pin(GpioExpander::GPIO_1, true);
    expander.set_pin(GpioExpander::GPIO_2, false);
    expander.set_pin(GpioExpander::GPIO_3, false);
    expander.set_pin(GpioExpander::GPIO_4, true);

    expander.Write();
    expander.Read();

    REQUIRE(expander.get_input(GpioExpander::GPIO_1) == true);
    REQUIRE(expander.get_input(GpioExpander::GPIO_2) == false);
    REQUIRE(expander.get_input(GpioExpander::GPIO_3) == false);
    REQUIRE(expander.get_input(GpioExpander::GPIO_4) == true);

    expander.set_pin(GpioExpander::GPIO_1, false);
    expander.set_pin(GpioExpander::GPIO_2, true);
    expander.set_pin(GpioExpander::GPIO_3, true);
    expander.set_pin(GpioExpander::GPIO_4, false);

    expander.Write();
    expander.Read();

    REQUIRE(expander.get_input(GpioExpander::GPIO_1) == false);
    REQUIRE(expander.get_input(GpioExpander::GPIO_2) == true);
    REQUIRE(expander.get_input(GpioExpander::GPIO_3) == true);
    REQUIRE(expander.get_input(GpioExpander::GPIO_4) == false);
  }
}

}  // namespace drivers
