/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database.hpp"

#include <stdint.h>
#include <iomanip>
#include <map>
#include <memory>
#include <string>

#include "catch2/catch.hpp"
#include "driver_cache.hpp"
#include "esp_log.h"
#include "file_gatherer.hpp"
#include "i2c_fixture.hpp"
#include "leveldb/db.h"
#include "spi_fixture.hpp"
#include "tag_parser.hpp"
#include "track.hpp"

namespace database {

class TestBackends : public IFileGatherer, public ITagParser {
 public:
  std::map<std::pmr::string, TrackTags> tracks;

  auto MakeTrack(const std::pmr::string& path, const std::pmr::string& title)
      -> void {
    TrackTags tags;
    tags.encoding = Encoding::kMp3;
    tags.title = title;
    tracks[path] = tags;
  }

  auto FindFiles(const std::pmr::string& root,
                 std::function<void(const std::pmr::string&)> cb)
      -> void override {
    for (auto keyval : tracks) {
      std::invoke(cb, keyval.first);
    }
  }

  auto ReadAndParseTags(const std::pmr::string& path, TrackTags* out)
      -> bool override {
    if (tracks.contains(path)) {
      *out = tracks.at(path);
      return true;
    }
    return false;
  }
};

TEST_CASE("track database", "[integration]") {
  I2CFixture i2c;
  SpiFixture spi;
  drivers::DriverCache drivers;
  auto storage = drivers.AcquireStorage();

  Database::Destroy();

  TestBackends tracks;
  auto open_res = Database::Open(&tracks, &tracks);
  REQUIRE(open_res.has_value());
  std::unique_ptr<Database> db(open_res.value());

  SECTION("empty database") {
    std::unique_ptr<Result<Track>> res(db->GetTracks(10).get());
    REQUIRE(res->values().size() == 0);
  }

  SECTION("add new tracks") {
    tracks.MakeTrack("track1.mp3", "Track 1");
    tracks.MakeTrack("track2.wav", "Track 2");
    tracks.MakeTrack("track3.exe", "Track 3");

    db->Update();

    std::unique_ptr<Result<Track>> res(db->GetTracks(10).get());
    REQUIRE(res->values().size() == 3);
    CHECK(*res->values().at(0).tags().title == "Track 1");
    CHECK(res->values().at(0).data().id() == 1);
    CHECK(*res->values().at(1).tags().title == "Track 2");
    CHECK(res->values().at(1).data().id() == 2);
    CHECK(*res->values().at(2).tags().title == "Track 3");
    CHECK(res->values().at(2).data().id() == 3);

    SECTION("update with no filesystem changes") {
      db->Update();

      std::unique_ptr<Result<Track>> new_res(db->GetTracks(10).get());
      REQUIRE(new_res->values().size() == 3);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(1) == new_res->values().at(1));
      CHECK(res->values().at(2) == new_res->values().at(2));
    }

    SECTION("update with all tracks gone") {
      tracks.tracks.clear();

      db->Update();

      std::unique_ptr<Result<Track>> new_res(db->GetTracks(10).get());
      CHECK(new_res->values().size() == 0);

      SECTION("update with one track returned") {
        tracks.MakeTrack("track2.wav", "Track 2");

        db->Update();

        std::unique_ptr<Result<Track>> new_res(db->GetTracks(10).get());
        REQUIRE(new_res->values().size() == 1);
        CHECK(res->values().at(1) == new_res->values().at(0));
      }
    }

    SECTION("update with one track gone") {
      tracks.tracks.erase("track2.wav");

      db->Update();

      std::unique_ptr<Result<Track>> new_res(db->GetTracks(10).get());
      REQUIRE(new_res->values().size() == 2);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(2) == new_res->values().at(1));
    }

    SECTION("update with tags changed") {
      tracks.MakeTrack("track3.exe", "The Track 3");

      db->Update();

      std::unique_ptr<Result<Track>> new_res(db->GetTracks(10).get());
      REQUIRE(new_res->values().size() == 3);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(1) == new_res->values().at(1));
      CHECK(*new_res->values().at(2).tags().title == "The Track 3");
      // The id should not have changed, since this was just a tag update.
      CHECK(res->values().at(2).data().id() ==
            new_res->values().at(2).data().id());
    }

    SECTION("update with one new track") {
      tracks.MakeTrack("my track.midi", "Track 1 (nightcore remix)");

      db->Update();

      std::unique_ptr<Result<Track>> new_res(db->GetTracks(10).get());
      REQUIRE(new_res->values().size() == 4);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(1) == new_res->values().at(1));
      CHECK(res->values().at(2) == new_res->values().at(2));
      CHECK(*new_res->values().at(3).tags().title ==
            "Track 1 (nightcore remix)");
      CHECK(new_res->values().at(3).data().id() == 4);
    }

    SECTION("get tracks with pagination") {
      std::unique_ptr<Result<Track>> res(db->GetTracks(1).get());

      REQUIRE(res->values().size() == 1);
      CHECK(res->values().at(0).data().id() == 1);
      REQUIRE(res->next_page());

      res.reset(db->GetPage(&res->next_page().value()).get());

      REQUIRE(res->values().size() == 1);
      CHECK(res->values().at(0).data().id() == 2);
      REQUIRE(res->next_page());

      res.reset(db->GetPage(&res->next_page().value()).get());

      REQUIRE(res->values().size() == 1);
      CHECK(res->values().at(0).data().id() == 3);
      REQUIRE(!res->next_page());

      SECTION("page backwards") {
        REQUIRE(res->prev_page());

        res.reset(db->GetPage(&res->prev_page().value()).get());

        REQUIRE(res->values().size() == 1);
        CHECK(res->values().at(0).data().id() == 2);
        REQUIRE(res->prev_page());

        res.reset(db->GetPage(&res->prev_page().value()).get());

        REQUIRE(res->values().size() == 1);
        CHECK(res->values().at(0).data().id() == 1);
        REQUIRE(!res->prev_page());

        SECTION("page forwards again") {
          REQUIRE(res->next_page());

          res.reset(db->GetPage(&res->next_page().value()).get());

          REQUIRE(res->values().size() == 1);
          CHECK(res->values().at(0).data().id() == 2);
          CHECK(res->next_page());
          CHECK(res->prev_page());
        }
      }
    }
  }
}

}  // namespace database
