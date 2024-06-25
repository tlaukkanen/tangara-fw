/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "catch2/catch.hpp"
#include "drivers/i2c.hpp"

class I2CFixture {
 public:
  I2CFixture() { REQUIRE(drivers::init_i2c() == ESP_OK); }
  ~I2CFixture() { drivers::deinit_i2c(); }
};
