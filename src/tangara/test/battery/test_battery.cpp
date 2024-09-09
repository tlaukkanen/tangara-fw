/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "battery/battery.hpp"

#include <cstdint>
#include <memory>

#include "catch2/catch.hpp"
#include "drivers/adc.hpp"
#include "i2c_fixture.hpp"

namespace battery {

class FakeAdc : public drivers::AdcBattery {
 private:
  uint32_t mv_;

 public:
  virtual auto Millivolts() -> uint32_t override { return mv_; }
  auto Millivolts(uint32_t mv) -> void { mv_ = mv; }
};

TEST_CASE("battery charge state", "[unit]") {
  I2CFixture i2c;
  std::unique_ptr<drivers::NvsStorage> nvs{drivers::NvsStorage::OpenSync()};

  // FIXME: mock the SAMD21 as well.
  auto samd = std::make_unique<drivers::Samd>(*nvs);
  FakeAdc* adc = new FakeAdc{};  // Freed by Battery.
  Battery battery{*samd, std::unique_ptr<drivers::AdcBattery>{adc}};

  SECTION("full charge is 100%") {
    // NOTE: in practice, our curve-fitting slightly undershoots
    adc->Millivolts(4210);

    battery.Update();

    auto state = battery.State();
    REQUIRE(state.has_value());
    REQUIRE(state->percent == 100);
  }

  SECTION("empty charge is 0%") {
    adc->Millivolts(3000);

    battery.Update();

    auto state = battery.State();
    REQUIRE(state.has_value());
    REQUIRE(state->percent == 0);
  }

  SECTION("overcharge is clamped to 100%") {
    adc->Millivolts(5000);

    battery.Update();

    auto state = battery.State();
    REQUIRE(state.has_value());
    REQUIRE(state->percent == 100);
  }
}

}  // namespace battery
