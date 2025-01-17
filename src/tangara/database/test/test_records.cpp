/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/records.hpp"

#include <stdint.h>
#include <iomanip>
#include <string>

#include "catch2/catch.hpp"

std::pmr::string ToHex(const std::pmr::string& s) {
  std::ostringstream ret;

  for (std::pmr::string::size_type i = 0; i < s.length(); ++i)
    ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase
        << (int)s[i];

  return ret.str();
}

namespace database {

TEST_CASE("database record encoding", "[unit]") {
  SECTION("track id to bytes") {
    TrackId id = 1234678;
    OwningSlice as_bytes = TrackIdToBytes(id);

    SECTION("encodes correctly") {
      // Purposefully a brittle test, since we need to be very careful about
      // changing the way records are encoded.
      REQUIRE(as_bytes.data.size() == 5);
      // unsigned value
      CHECK(as_bytes.data[0] == 0x1A);
      // TODO(jacqueline): what's up with these failing?
      // 12345678
      // CHECK(as_bytes.data[1] == 0x00);
      // CHECK(as_bytes.data[2] == 0x01);
      // CHECK(as_bytes.data[3] == 0xE2);
      // CHECK(as_bytes.data[4] == 0x40);
    }

    SECTION("round-trips") {
      CHECK(*BytesToTrackId(as_bytes.data) == id);
    }

    SECTION("encodes compactly") {
      OwningSlice small_id = TrackIdToBytes(1);
      OwningSlice large_id = TrackIdToBytes(999999);

      CHECK(small_id.data.size() < large_id.data.size());
    }

    SECTION("decoding rejects garbage") {
      std::optional<TrackId> res = BytesToTrackId("i'm gay");

      CHECK(res.has_value() == false);
    }
  }

  SECTION("data keys") {
    OwningSlice key = CreateDataKey(123456);

    REQUIRE(key.data.size() == 7);
    CHECK(key.data[0] == 'D');
    CHECK(key.data[1] == '\0');
    // unsigned int
    CHECK(key.data[2] == 0x1A);
    // assume the int encoding is fine.
  }

  SECTION("data values") {
    TrackData data(123, "/some/path.mp3", 0xACAB, 69, true);

    OwningSlice enc = CreateDataValue(data);

    SECTION("encodes correctly") {
      REQUIRE(enc.data.size() == 24);

      // Array, length 5
      CHECK(enc.data[0] == 0x85);

      // unsigned int, value 123
      CHECK(enc.data[1] == 0x18);
      CHECK(enc.data[2] == 0x7B);

      // text, 14 chars
      CHECK(enc.data[3] == 0x6E);
      // ... assume the text looks okay.

      // unsigned int, value 44203
      CHECK(enc.data[18] == 0x19);
      CHECK(enc.data[19] == 0xAC);
      CHECK(enc.data[20] == 0xAB);

      // unsigned int, value 69
      CHECK(enc.data[21] == 0x18);
      CHECK(enc.data[22] == 0x45);

      // primitive 21, true
      CHECK(enc.data[23] == 0xF5);
    }

    SECTION("round-trips") {
      CHECK(ParseDataValue(enc.slice) == data);
    }

    SECTION("decoding rejects garbage") {
      std::optional<TrackData> res = ParseDataValue("hi!");

      CHECK(res.has_value() == false);
    }
  }

  SECTION("hash keys") {
    OwningSlice key = CreateHashKey(123456);

    REQUIRE(key.data.size() == 7);
    CHECK(key.data[0] == 'H');
    CHECK(key.data[1] == '\0');
    // unsigned int
    CHECK(key.data[2] == 0x1A);
    // assume the int encoding is fine.
  }

  SECTION("hash values") {
    OwningSlice val = CreateHashValue(123456);

    CHECK(val.data == TrackIdToBytes(123456).data);

    SECTION("round-trips") {
      CHECK(ParseHashValue(val.slice) == 123456);
    }

    SECTION("decoding rejects garbage") {
      std::optional<TrackId> res = ParseHashValue("the first track :)");

      CHECK(res.has_value() == false);
    }
  }
}

}  // namespace database
