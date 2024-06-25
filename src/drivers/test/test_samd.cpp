/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/samd.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

#include "i2c_fixture.hpp"

namespace drivers {

TEST_CASE("samd21 interface", "[integration]") {
  I2CFixture i2c;
  auto samd = std::make_unique<Samd>();

  REQUIRE(samd);

  SECTION("usb reports connection") {
    samd->UpdateUsbStatus();

    auto status = samd->GetUsbStatus();

    REQUIRE(status == Samd::UsbStatus::kAttachedIdle);
  }
}

}  // namespace drivers
