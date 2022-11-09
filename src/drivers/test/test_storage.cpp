#include <dirent.h>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "gpio-expander.hpp"
#include "i2c.hpp"
#include "i2c_fixture.hpp"
#include "spi.hpp"
#include "spi_fixture.hpp"
#include "storage.hpp"

#include "catch2/catch.hpp"

namespace drivers {

static const std::string kTestFilename = "test";
static const std::string kTestFilePath =
    std::string(kStoragePath) + "/" + kTestFilename;

TEST_CASE("sd card storage", "[integration]") {
  I2CFixture i2c;
  SpiFixture spi;
  GpioExpander expander;

  {
    std::unique_ptr<SdStorage> result = SdStorage::create(&expander).value();

    SECTION("write to a file") {
      {
        std::ofstream test_file;
        test_file.open(kTestFilePath.c_str());
        test_file << "hello here is some test";
        test_file.close();
      }

      SECTION("read from a file") {
        std::ifstream test_file;
        test_file.open(kTestFilePath.c_str());

        std::string line;
        REQUIRE(std::getline(test_file, line));
        REQUIRE(line == "hello here is some test");

        test_file.close();
      }

      SECTION("list files") {
        DIR* dir;
        struct dirent* ent;

        dir = opendir(kStoragePath);
        REQUIRE(dir != nullptr);

        bool found_test_file = false;
        while (ent = readdir(dir)) {
          if (ent->d_name == kTestFilename) {
            found_test_file = true;
          }
        }
        closedir(dir);

        REQUIRE(found_test_file);
      }

      REQUIRE(remove(kTestFilePath.c_str()) == 0);
    }
  }
}

// Failing due to hardware issue. Re-enable in R2.
TEST_CASE("sd card mux", "[integration][!mayfail]") {
  I2CFixture i2c;
  SpiFixture spi;
  GpioExpander expander;

  SECTION("accessible when switched on") {
    expander.with([&](auto& gpio) {
      gpio.set_pin(GpioExpander::SD_MUX_SWITCH, GpioExpander::SD_MUX_ESP);
    });

    auto result = SdStorage::create(&expander);
    REQUIRE(result.has_value());
  }

  SECTION("inaccessible when switched off") {
    expander.with([&](auto& gpio) {
      gpio.set_pin(GpioExpander::SD_MUX_SWITCH, GpioExpander::SD_MUX_USB);
    });

    auto result = SdStorage::create(&expander);
    REQUIRE(result.has_error());
    REQUIRE(result.error() == SdStorage::FAILED_TO_READ);
  }
}

}  // namespace drivers
