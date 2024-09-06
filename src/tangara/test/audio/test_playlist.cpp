/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/playlist.hpp"

#include <dirent.h>

#include <cstdio>

#include "catch2/catch.hpp"

#include "drivers/gpios.hpp"
#include "drivers/i2c.hpp"
#include "drivers/spi.hpp"
#include "drivers/storage.hpp"
#include "ff.h"
#include "i2c_fixture.hpp"
#include "spi_fixture.hpp"

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
    std::unique_ptr<drivers::SdStorage> result(
        drivers::SdStorage::Create(*gpios).value());
    MutablePlaylist plist(kTestFilePath);

    SECTION("empty file appears empty") {
      REQUIRE(plist.clear());

      REQUIRE(plist.size() == 0);
      REQUIRE(plist.currentPosition() == 0);
      REQUIRE(plist.value().empty());
    }

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
        REQUIRE(plist2.open());
        REQUIRE(plist2.size() == 8);
        REQUIRE(plist2.value() == "test1.mp3");
        plist2.next();
        REQUIRE(plist2.value() == "test2.mp3");
        plist2.prev();
        REQUIRE(plist2.value() == "test1.mp3");
      }
    }

    REQUIRE(plist.clear());

    size_t tracks = 0;

    BENCHMARK("appending items") {
      plist.append("track " + std::to_string(plist.size()));
      return tracks++;
    };

    BENCHMARK("opening large playlist file") {
      Playlist plist2(kTestFilePath);
      REQUIRE(plist2.open());
      REQUIRE(plist2.size() == tracks);
      return plist2.size();
    };

    BENCHMARK("seeking after appending a large file") {
      REQUIRE(plist.size() == tracks);

      plist.skipTo(50);
      REQUIRE(plist.value() == "track 50");
      plist.skipTo(99);
      REQUIRE(plist.value() == "track 99");
      plist.skipTo(1);
      REQUIRE(plist.value() == "track 1");

      return plist.size();
    };

    BENCHMARK("seeking after opening a large file") {
      Playlist plist2(kTestFilePath);
      REQUIRE(plist2.open());
      REQUIRE(plist.size() == tracks);
      REQUIRE(tracks >= 100);

      plist.skipTo(50);
      REQUIRE(plist.value() == "track 50");
      plist.skipTo(99);
      REQUIRE(plist.value() == "track 99");
      plist.skipTo(1);
      REQUIRE(plist.value() == "track 1");

      return plist.size();
    };

    BENCHMARK("opening a large file and appending") {
      MutablePlaylist plist2(kTestFilePath);
      REQUIRE(plist2.open());
      REQUIRE(plist2.size() >= 100);
      plist2.append("A/Nother/New/Item.opus");
      return plist2.size();
    };
  }
}
}  // namespace audio
