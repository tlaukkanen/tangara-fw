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
  std::unique_ptr<AudioDac> dac = AudioDac::create(&expander).value();

  auto power_state = dac->ReadPowerState();

  REQUIRE(power_state.first == true);  // booted
}

}  // namespace drivers
