#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "cbor.h"
#include "result.hpp"
#include "sys/_stdint.h"

namespace audio {

struct StreamInfo {
  std::optional<std::string> path;
  std::optional<uint8_t> channels;
  std::optional<uint8_t> bits_per_sample;
  std::optional<uint16_t> sample_rate;
  std::optional<size_t> chunk_size;
};

}  // namespace audio
