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
#include <span>
#include <string>
#include <utility>

#include "ivorbisfile.h"
#include "sample.hpp"

#include "codec.hpp"

namespace codecs {

class TremorVorbisDecoder : public ICodec {
 public:
  TremorVorbisDecoder();
  ~TremorVorbisDecoder();

  auto OpenStream(std::shared_ptr<IStream> input, uint32_t offset)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(std::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  TremorVorbisDecoder(const TremorVorbisDecoder&) = delete;
  TremorVorbisDecoder& operator=(const TremorVorbisDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  std::unique_ptr<TremorOggVorbis_File> vorbis_;
};

}  // namespace codecs
