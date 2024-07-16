/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/playlist.hpp"

#include <dirent.h>

#include <cstdio>
#include <fstream>
#include <iostream>

#include "catch2/catch.hpp"

#include "drivers/gpios.hpp"
#include "drivers/i2c.hpp"
#include "drivers/storage.hpp"
#include "drivers/spi.hpp"
#include "i2c_fixture.hpp"
#include "spi_fixture.hpp"
#include "ff.h"

namespace audio {

static const std::string kTestFilename = "test_playlist2.m3u";
static const std::string kTestFilePath = kTestFilename;

TEST_CASE("playlist file", "[integration]") {
  I2CFixture i2c;
  SpiFixture spi;
  std::unique_ptr<drivers::IGpios> gpios{drivers::Gpios::Create(false)};

  if (gpios->Get(drivers::IGpios::Pin::kSdCardDetect)) {
    // Skip if nothing is inserted.
    SKIP("no sd card detected; skipping storage tests");
    return;
  }

  {
    std::unique_ptr<drivers::SdStorage> result(drivers::SdStorage::Create(*gpios).value());
    Playlist plist(kTestFilePath);
    REQUIRE(plist.clear());

    SECTION("write to the playlist file") {
      plist.append("test1.mp3");
      plist.append("test2.mp3");
      plist.append("test3.mp3");
      plist.append("test4.wav");
      plist.append("directory/test1.mp3");
      plist.append("directory/test2.mp3");
      plist.append("a/really/long/directory/test1.mp3");
      plist.append("directory/and/another/test2.mp3");
      REQUIRE(plist.size() == 8);

      SECTION("read from the playlist file") {
        Playlist plist2(kTestFilePath);
        REQUIRE(plist2.size() == 8);
        REQUIRE(plist2.value() == "test1.mp3");
        plist2.next();
        REQUIRE(plist2.value() == "test2.mp3");
        plist2.prev();
        REQUIRE(plist2.value() == "test1.mp3");
      }
    }

    BENCHMARK("appending item") {
      plist.append("A/New/Item.wav");
    };

    BENCHMARK("opening playlist file") {
      Playlist plist2(kTestFilePath);
      REQUIRE(plist2.size() > 100);
      return plist2.size();
    };

    BENCHMARK("opening playlist file and appending entry") {
      Playlist plist2(kTestFilePath);
      REQUIRE(plist2.size() > 100);
      plist2.append("A/Nother/New/Item.opus");
      return plist2.size();
    };
  }
}
} // namespace audio
