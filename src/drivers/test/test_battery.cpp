#include "battery.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

namespace drivers {

TEST_CASE("battery measurement", "[integration]") {
  REQUIRE(drivers::init_adc() == ESP_OK);

  SECTION("voltage is within range") {
    uint32_t voltage = read_battery_voltage();
    REQUIRE(voltage <= 2200);  // Plugged in, no battery.
    REQUIRE(voltage >= 1000);
  }
}

}  // namespace drivers
