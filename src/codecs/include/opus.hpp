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

#include "opusfile.h"
#include "sample.hpp"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class XiphOpusDecoder : public ICodec {
 public:
  XiphOpusDecoder();
  ~XiphOpusDecoder();

  auto OpenStream(std::shared_ptr<IStream> input,uint32_t offset)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(cpp::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  auto SeekTo(std::size_t target_sample) -> cpp::result<void, Error> override;

  XiphOpusDecoder(const XiphOpusDecoder&) = delete;
  XiphOpusDecoder& operator=(const XiphOpusDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  OggOpusFile* opus_;
  uint8_t num_channels_;
};

}  // namespace codecs
