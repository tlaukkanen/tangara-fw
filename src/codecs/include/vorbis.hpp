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

#include "ivorbisfile.h"
#include "ogg.hpp"
#include "ogg/ogg.h"
#include "opus.h"
#include "sample.hpp"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class TremorVorbisDecoder : public ICodec {
 public:
  TremorVorbisDecoder();
  ~TremorVorbisDecoder();

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

  auto ReadCallback() -> cpp::span<const std::byte>;
  auto AfterReadCallback(size_t bytes_read) -> void;

 private:
  OggVorbis_File vorbis_;
  cpp::span<const std::byte> input_;
  size_t pos_in_input_;
};

}  // namespace codecs
