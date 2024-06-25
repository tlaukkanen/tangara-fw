/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "catch2/catch.hpp"
#include "drivers/spi.hpp"

class SpiFixture {
 public:
  SpiFixture() { REQUIRE(drivers::init_spi() == ESP_OK); }
  ~SpiFixture() { drivers::deinit_spi(); }
};
