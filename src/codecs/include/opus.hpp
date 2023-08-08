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

#include "opus.h"
#include "sample.hpp"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class XiphOpusDecoder : public ICodec {
 public:
  XiphOpusDecoder();
  ~XiphOpusDecoder();

  /*
   * Returns the output format for the next frame in the stream. MP3 streams
   * may represent multiple distinct tracks, with different bitrates, and so we
   * handle the stream only on a frame-by-frame basis.
   */
  auto BeginStream(cpp::span<const std::byte>) -> Result<OutputFormat> override;

  /*
   * Writes samples for the current frame.
   */
  auto ContinueStream(cpp::span<const std::byte> input,
                      cpp::span<sample::Sample> output)
      -> Result<OutputInfo> override;

  auto SeekStream(cpp::span<const std::byte> input, std::size_t target_sample)
      -> Result<void> override;

 private:
  OpusDecoder* opus_;
  cpp::span<int16_t> sample_buffer_;
  int32_t pos_in_buffer_;
  int32_t samples_in_buffer_;

};

}  // namespace codecs
