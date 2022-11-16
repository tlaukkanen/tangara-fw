#pragma once

#include "catch2/catch.hpp"
#include "spi.hpp"

class SpiFixture {
 public:
  SpiFixture() { REQUIRE(drivers::init_spi() == ESP_OK); }
  ~SpiFixture() { drivers::deinit_spi(); }
};
