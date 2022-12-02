#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cbor.h"
#include "result.hpp"

namespace audio {

class StreamInfo {
 public:
  static auto Parse(CborValue& container) -> cpp::result<StreamInfo, CborError>;

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

  auto Encode(CborEncoder& enc) -> std::optional<CborError>;

 private:
  std::optional<std::string> path_;
  std::optional<uint8_t> channels_;
  std::optional<uint8_t> bits_per_sample_;
  std::optional<uint16_t> sample_rate_;
};

}  // namespace audio
