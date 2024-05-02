/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "codec.hpp"
#include "ff.h"

#include "audio_source.hpp"

namespace audio {

/*
 * Handles coordination with a persistent background task to asynchronously
 * read files from disk into a StreamBuffer.
 */
class FatfsSource : public codecs::IStream {
 public:
  FatfsSource(codecs::StreamType, std::unique_ptr<FIL> file);
  ~FatfsSource();

  auto Read(std::span<std::byte> dest) -> ssize_t override;

  auto CanSeek() -> bool override;

  auto SeekTo(int64_t destination, SeekFrom from) -> void override;

  auto CurrentPosition() -> int64_t override;

  auto Size() -> std::optional<int64_t> override;

  FatfsSource(const FatfsSource&) = delete;
  FatfsSource& operator=(const FatfsSource&) = delete;

 private:
  std::unique_ptr<FIL> file_;
};

}  // namespace audio
