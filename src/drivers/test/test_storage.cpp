/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "storage.hpp"

#include <dirent.h>

#include <cstdio>
#include <fstream>
#include <iostream>

#include "catch2/catch.hpp"

#include "gpios.hpp"
#include "i2c.hpp"
#include "i2c_fixture.hpp"
#include "spi.hpp"
#include "spi_fixture.hpp"

namespace drivers {

static const std::string kTestFilename = "test";
static const std::string kTestFilePath =
    std::string(kStoragePath) + "/" + kTestFilename;

TEST_CASE("sd card storage", "[integration]") {
  I2CFixture i2c;
  SpiFixture spi;
  IGpios expander;

  {
    std::unique_ptr<SdStorage> result(SdStorage::create(&expander).value());

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

}  // namespace drivers
