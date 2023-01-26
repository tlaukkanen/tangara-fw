#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "cbor.h"
#include "result.hpp"
#include "sys/_stdint.h"

namespace audio {

class StreamInfo {
 public:
  static auto Parse(CborValue& container) -> cpp::result<StreamInfo, CborError>;

  StreamInfo() = default;
  StreamInfo(const StreamInfo&) = default;

  ~StreamInfo() = default;

  auto Path() const -> const std::optional<std::string>& { return path_; }
  auto Path(const std::string_view& d) -> void { path_ = d; }

  auto Channels() const -> const std::optional<uint8_t>& { return channels_; }

  auto BitsPerSample(uint8_t bpp) -> void { bits_per_sample_ = bpp; }

  auto BitsPerSample() const -> const std::optional<uint8_t>& {
    return bits_per_sample_;
  }

  auto SampleRate(uint16_t rate) -> void { sample_rate_ = rate; }

  auto SampleRate() const -> const std::optional<uint16_t>& {
    return sample_rate_;
  }

  auto ChunkSize() const -> const std::optional<std::size_t>& {
    return chunk_size_;
  }

  auto ChunkSize(std::size_t s) -> void { chunk_size_ = s; }

  auto Encode(CborEncoder& enc) -> std::optional<CborError>;

 private:
  std::optional<std::string> path_;
  std::optional<uint8_t> channels_;
  std::optional<uint8_t> bits_per_sample_;
  std::optional<uint16_t> sample_rate_;
  std::optional<size_t> chunk_size_;
};

}  // namespace audio
