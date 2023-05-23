/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "mad.hpp"

#include <algorithm>
#include <cstdint>

#include "catch2/catch.hpp"
#include "span.hpp"

#include "test.mp3.hpp"

void load_mp3(cpp::span<std::byte> dest) {
  cpp::span<std::byte> src(reinterpret_cast<std::byte*>(test_mp3),
                           test_mp3_len);
  std::copy(src.begin(), src.begin() + dest.size(), dest.begin());
}

namespace codecs {

TEST_CASE("libmad mp3 decoder", "[unit]") {
  MadMp3Decoder decoder;

  SECTION("handles only mp3 types") {
    REQUIRE(decoder.CanHandleType(STREAM_MP3) == true);
    // REQUIRE(decoder.CanHandleFile("bad.wma") == false);
  }

  SECTION("processes streams correctly") {
    std::array<std::byte, 2048> input;
    input.fill(std::byte{0});

    decoder.ResetForNewStream();
    decoder.SetInput(input);

    SECTION("empty stream fails gracefully") {
      auto result = decoder.ProcessNextFrame();

      REQUIRE(result.has_value());
      REQUIRE(result.value() == true);
    }

    SECTION("valid stream parses successfully") {
      load_mp3(input);

      auto result = decoder.ProcessNextFrame();

      REQUIRE(result.has_value());
      REQUIRE(result.value() == false);

      SECTION("output format correct") {
        auto format = decoder.GetOutputFormat();

        // Matches the test data.
        REQUIRE(format.num_channels == 1);
        REQUIRE(format.sample_rate_hz == 44100);
        // Matches libmad output
        REQUIRE(format.bits_per_sample == 16);
      }
    }
  }
}

}  // namespace codecs
