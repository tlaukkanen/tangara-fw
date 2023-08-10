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

  auto OpenStream(std::shared_ptr<IStream> input)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(cpp::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  auto SeekTo(std::size_t target_sample) -> cpp::result<void, Error> override;

  TremorVorbisDecoder(const TremorVorbisDecoder&) = delete;
  TremorVorbisDecoder& operator=(const TremorVorbisDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  OggVorbis_File vorbis_;
};

}  // namespace codecs
