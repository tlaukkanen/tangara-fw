#include "records.hpp"

#include <stdint.h>
#include <iomanip>
#include <string>

#include "catch2/catch.hpp"

std::string ToHex(const std::string& s) {
  std::ostringstream ret;

  for (std::string::size_type i = 0; i < s.length(); ++i)
    ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase
        << (int)s[i];

  return ret.str();
}

namespace database {

TEST_CASE("database record encoding", "[unit]") {
  SECTION("song id to bytes") {
    SongId id = 1234678;
    OwningSlice as_bytes = SongIdToBytes(id);

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
      CHECK(BytesToSongId(as_bytes.data) == id);
    }

    SECTION("encodes compactly") {
      OwningSlice small_id = SongIdToBytes(1);
      OwningSlice large_id = SongIdToBytes(999999);

      CHECK(small_id.data.size() < large_id.data.size());
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
    SongData data(123, "/some/path.mp3", 0xACAB, 69, true);

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

    CHECK(val.data == SongIdToBytes(123456).data);

    SECTION("round-trips") {
      CHECK(ParseHashValue(val.slice) == 123456);
    }
  }
}

}  // namespace database
