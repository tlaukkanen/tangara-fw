/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "codec.hpp"
#include "ff.h"

#include "audio/audio_source.hpp"

namespace audio {

/*
 * Generates an infinitely long sine wave of a specified frequency.
 */
class SineSource : public codecs::IStream {
 public:
  SineSource(uint32_t frequency);

  auto Read(std::span<std::byte> dest) -> ssize_t override;

  auto CanSeek() -> bool override;

  auto SeekTo(int64_t destination, SeekFrom from) -> void override;

  auto CurrentPosition() -> int64_t override;

  auto Size() -> std::optional<int64_t> override;

  SineSource(const SineSource&) = delete;
  SineSource& operator=(const SineSource&) = delete;

 private:
  double step_;
  double increment_;
};

}  // namespace audio
