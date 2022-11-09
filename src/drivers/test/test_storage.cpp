#include <dirent.h>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "gpio-expander.hpp"
#include "storage.hpp"
#include "i2c.hpp"
#include "spi.hpp"

#include "catch2/catch.hpp"

namespace drivers {

static const std::string kTestFilename = "test";
static const std::string kTestFilePath =
    std::string(kStoragePath) + "/" + kTestFilename;

TEST_CASE("sd card storage", "[integration]") {
  REQUIRE( drivers::init_i2c() == ESP_OK );
  REQUIRE( drivers::init_spi() == ESP_OK );
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

  REQUIRE( drivers::deinit_i2c() == ESP_OK );
  REQUIRE( drivers::deinit_spi() == ESP_OK );
}

}  // namespace drivers
