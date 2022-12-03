#include "mad.hpp"

#include <cstdint>
#include <algorithm>

#include "span.hpp"
#include "catch2/catch.hpp"

#include "test.mp3.hpp"

void load_mp3(cpp::span<std::byte> dest) {
  cpp::span<std::byte> src(reinterpret_cast<std::byte*>(test_mp3), test_mp3_len);
  std::copy(src.begin(), src.begin() + dest.size(), dest.begin());
}

namespace codecs {

TEST_CASE("libmad mp3 decoder", "[unit]") {
  MadMp3Decoder decoder;

  SECTION("handles only mp3 files") {
    REQUIRE(decoder.CanHandleFile("cool.mp3") == true);
    REQUIRE(decoder.CanHandleFile("bad.wma") == false);
  }

  SECTION("processes streams correctly") {
    std::array<std::byte, 256> input;
    input.fill(std::byte{0});

    decoder.ResetForNewStream();
    decoder.SetInput(input);

    SECTION("empty stream fails gracefully") {
      auto result = decoder.ProcessNextFrame();

      REQUIRE(result.has_value());
      REQUIRE(result.value() == true);
    }

    SECTION("invalid stream fails") {
      input.fill(std::byte{0xFF});

      auto result = decoder.ProcessNextFrame();

      REQUIRE(result.has_error());
      REQUIRE(result.error() == ICodec::MALFORMED_DATA);
    }

    SECTION("valid stream parses successfully") {
      load_mp3(input);

      auto result = decoder.ProcessNextFrame();

      REQUIRE(result.has_value());
      REQUIRE(result.value() == false);

      SECTION("output samples synthesized") {
        std::array<std::byte, 256> output;
        output.fill(std::byte{0});

        auto res = decoder.WriteOutputSamples(output);

        REQUIRE(res.first > 0);
        REQUIRE(res.second == false);

        // Just check that some kind of data was written. We don't care
        // about what.
        REQUIRE(std::to_integer<uint8_t>(output[0]) != 0);
        REQUIRE(std::to_integer<uint8_t>(output[1]) != 0);
        REQUIRE(std::to_integer<uint8_t>(output[2]) != 0);
        REQUIRE(std::to_integer<uint8_t>(output[3]) != 0);
      }

      SECTION("output format correct") {
        auto format = decoder.GetOutputFormat();

        // Matches the test data.
        REQUIRE(format.num_channels == 2);
        REQUIRE(format.sample_rate_hz == 44100);
        // Matches libmad output
        REQUIRE(format.bits_per_sample == 24);
      }
    }

  }
}

}  // namespace codecs
