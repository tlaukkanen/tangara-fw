#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cbor.h"
#include "result.hpp"

#include "cbor_decoder.hpp"
#include "cbor_encoder.hpp"

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
  StreamInfo(cbor::MapDecoder*);

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

  auto WriteToMap(cbor::Encoder& encoder) -> cpp::result<size_t, CborError>;

 private:
  std::optional<std::string> path_;
  std::optional<uint8_t> channels_;
  std::optional<uint8_t> bits_per_sample_;
  std::optional<uint16_t> sample_rate_;
};

}  // namespace audio
