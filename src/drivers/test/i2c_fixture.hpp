#pragma once

#include "catch2/catch.hpp"
#include "i2c.hpp"

class I2CFixture {
 public:
  I2CFixture() { REQUIRE(drivers::init_i2c() == ESP_OK); }
  ~I2CFixture() { drivers::deinit_i2c(); }
};
