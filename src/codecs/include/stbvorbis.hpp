/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "stb_vorbis.h"

#include "codec.hpp"

namespace codecs {

class StbVorbisDecoder : public ICodec {
 public:
  StbVorbisDecoder();
  ~StbVorbisDecoder();

  auto BeginStream(cpp::span<const std::byte>) -> Result<OutputFormat> override;
  auto ContinueStream(cpp::span<const std::byte>, cpp::span<std::byte>)
      -> Result<OutputInfo> override;
  auto SeekStream(cpp::span<const std::byte> input, std::size_t target_sample)
      -> Result<void> override;

 private:
  stb_vorbis* vorbis_;

  int current_sample_;
  int num_channels_;
  int num_samples_;
  float** samples_array_;
};

}  // namespace codecs
