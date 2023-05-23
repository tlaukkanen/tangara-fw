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
#include "song.hpp"
#include "spi_fixture.hpp"
#include "tag_parser.hpp"

namespace database {

class TestBackends : public IFileGatherer, public ITagParser {
 public:
  std::map<std::string, SongTags> songs;

  auto MakeSong(const std::string& path, const std::string& title) -> void {
    SongTags tags;
    tags.encoding = Encoding::kMp3;
    tags.title = title;
    songs[path] = tags;
  }

  auto FindFiles(const std::string& root,
                 std::function<void(const std::string&)> cb) -> void override {
    for (auto keyval : songs) {
      std::invoke(cb, keyval.first);
    }
  }

  auto ReadAndParseTags(const std::string& path, SongTags* out)
      -> bool override {
    if (songs.contains(path)) {
      *out = songs.at(path);
      return true;
    }
    return false;
  }
};

TEST_CASE("song database", "[integration]") {
  I2CFixture i2c;
  SpiFixture spi;
  drivers::DriverCache drivers;
  auto storage = drivers.AcquireStorage();

  Database::Destroy();

  TestBackends songs;
  auto open_res = Database::Open(&songs, &songs);
  REQUIRE(open_res.has_value());
  std::unique_ptr<Database> db(open_res.value());

  SECTION("empty database") {
    std::unique_ptr<Result<Song>> res(db->GetSongs(10).get());
    REQUIRE(res->values().size() == 0);
  }

  SECTION("add new songs") {
    songs.MakeSong("song1.mp3", "Song 1");
    songs.MakeSong("song2.wav", "Song 2");
    songs.MakeSong("song3.exe", "Song 3");

    db->Update();

    std::unique_ptr<Result<Song>> res(db->GetSongs(10).get());
    REQUIRE(res->values().size() == 3);
    CHECK(*res->values().at(0).tags().title == "Song 1");
    CHECK(res->values().at(0).data().id() == 1);
    CHECK(*res->values().at(1).tags().title == "Song 2");
    CHECK(res->values().at(1).data().id() == 2);
    CHECK(*res->values().at(2).tags().title == "Song 3");
    CHECK(res->values().at(2).data().id() == 3);

    SECTION("update with no filesystem changes") {
      db->Update();

      std::unique_ptr<Result<Song>> new_res(db->GetSongs(10).get());
      REQUIRE(new_res->values().size() == 3);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(1) == new_res->values().at(1));
      CHECK(res->values().at(2) == new_res->values().at(2));
    }

    SECTION("update with all songs gone") {
      songs.songs.clear();

      db->Update();

      std::unique_ptr<Result<Song>> new_res(db->GetSongs(10).get());
      CHECK(new_res->values().size() == 0);

      SECTION("update with one song returned") {
        songs.MakeSong("song2.wav", "Song 2");

        db->Update();

        std::unique_ptr<Result<Song>> new_res(db->GetSongs(10).get());
        REQUIRE(new_res->values().size() == 1);
        CHECK(res->values().at(1) == new_res->values().at(0));
      }
    }

    SECTION("update with one song gone") {
      songs.songs.erase("song2.wav");

      db->Update();

      std::unique_ptr<Result<Song>> new_res(db->GetSongs(10).get());
      REQUIRE(new_res->values().size() == 2);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(2) == new_res->values().at(1));
    }

    SECTION("update with tags changed") {
      songs.MakeSong("song3.exe", "The Song 3");

      db->Update();

      std::unique_ptr<Result<Song>> new_res(db->GetSongs(10).get());
      REQUIRE(new_res->values().size() == 3);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(1) == new_res->values().at(1));
      CHECK(*new_res->values().at(2).tags().title == "The Song 3");
      // The id should not have changed, since this was just a tag update.
      CHECK(res->values().at(2).data().id() ==
            new_res->values().at(2).data().id());
    }

    SECTION("update with one new song") {
      songs.MakeSong("my song.midi", "Song 1 (nightcore remix)");

      db->Update();

      std::unique_ptr<Result<Song>> new_res(db->GetSongs(10).get());
      REQUIRE(new_res->values().size() == 4);
      CHECK(res->values().at(0) == new_res->values().at(0));
      CHECK(res->values().at(1) == new_res->values().at(1));
      CHECK(res->values().at(2) == new_res->values().at(2));
      CHECK(*new_res->values().at(3).tags().title ==
            "Song 1 (nightcore remix)");
      CHECK(new_res->values().at(3).data().id() == 4);
    }

    SECTION("get songs with pagination") {
      std::unique_ptr<Result<Song>> res(db->GetSongs(1).get());

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