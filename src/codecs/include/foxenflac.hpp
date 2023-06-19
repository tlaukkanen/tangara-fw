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

#include "foxen/flac.h"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class FoxenFlacDecoder : public ICodec {
 public:
  FoxenFlacDecoder();
  ~FoxenFlacDecoder();

  auto BeginStream(cpp::span<const std::byte>) -> Result<OutputFormat> override;
  auto ContinueStream(cpp::span<const std::byte>, cpp::span<std::byte>)
      -> Result<OutputInfo> override;
  auto SeekStream(cpp::span<const std::byte> input, std::size_t target_sample)
      -> Result<void> override;

 private:
  fx_flac_t* flac_;
};

}  // namespace codecs
