#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "result.hpp"

namespace audio {

class StreamInfo {
 public:
  enum ParseError {
    WRONG_TYPE,
    MISSING_MAP,
    CBOR_ERROR,
  };

  static auto Create(const uint8_t* buffer, size_t length)
      -> cpp::result<StreamInfo, ParseError>;
  StreamInfo(CborValue& map);

  StreamInfo() = default;
  StreamInfo(const StreamInfo&) = default;

  ~StreamInfo() = default;

  auto Path() const -> const std::optional<std::string>& { return path_; }
  auto Channels() const -> const std::optional<uint8_t>& { return channels_; }
  auto BitsPerSample() const -> const std::optional<uint8_t>& {
    return bits_per_sample_;
  }
  auto SampleRate() const -> const std::optional<uint16_t>& {
    return sample_rate_;
  }

  enum EncodeError {
    OUT_OF_MEMORY,
  };

  auto WriteToMap(CborEncoder encoder) -> cpp::result<size_t, EncodeError>;

 private:
  std::optional<std::string> path_;
  std::optional<uint8_t> channels_;
  std::optional<uint8_t> bits_per_sample_;
  std::optional<uint16_t> sample_rate_;
};

}  // namespace audio
