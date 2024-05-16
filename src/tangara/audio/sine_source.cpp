/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/sine_source.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>

#include "esp_log.h"

#include "audio/audio_source.hpp"
#include "codec.hpp"
#include "drivers/spi.hpp"
#include "sample.hpp"
#include "system_fsm/system_events.hpp"
#include "types.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "sine_src";

SineSource::SineSource(uint32_t freq)
    : IStream(codecs::StreamType::kNative),
      step_(0),
      increment_((2.0 * std::numbers::pi) / (48000.0 / freq)) {}

auto SineSource::Read(std::span<std::byte> dest_bytes) -> ssize_t {
  std::span<sample::Sample> dest{
      reinterpret_cast<sample::Sample*>(dest_bytes.data()),
      dest_bytes.size_bytes() / sizeof(sample::Sample)};

  for (size_t i = 0; i < dest.size(); i++) {
    dest[i] = std::numeric_limits<sample::Sample>::max() *
              std::sin(step_ += increment_);
  }

  return dest.size_bytes();
}

auto SineSource::CanSeek() -> bool {
  return false;
}

auto SineSource::SeekTo(int64_t destination, SeekFrom from) -> void {}

auto SineSource::CurrentPosition() -> int64_t {
  return 0;
}

auto SineSource::Size() -> std::optional<int64_t> {
  return {};
}

}  // namespace audio
