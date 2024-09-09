/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/nvs.hpp"
#include "drivers/samd.hpp"

#include <cstdint>

#include "catch2/catch.hpp"

#include "i2c_fixture.hpp"

namespace drivers {

TEST_CASE("samd21 interface", "[integration]") {
  I2CFixture i2c;
  std::unique_ptr<drivers::NvsStorage> nvs{drivers::NvsStorage::OpenSync()};
  auto samd = std::make_unique<Samd>(*nvs);

  REQUIRE(samd);

  SECTION("usb reports connection") {
    samd->UpdateUsbStatus();

    auto status = samd->GetUsbStatus();

    REQUIRE(status == Samd::UsbStatus::kAttachedIdle);
  }
}

}  // namespace drivers
