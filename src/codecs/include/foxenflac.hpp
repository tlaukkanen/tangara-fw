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
#include "sample.hpp"
#include "source_buffer.hpp"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class FoxenFlacDecoder : public ICodec {
 public:
  FoxenFlacDecoder();
  ~FoxenFlacDecoder();

  auto OpenStream(std::shared_ptr<IStream> input)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(cpp::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  auto SeekTo(std::size_t target_sample) -> cpp::result<void, Error> override;

  FoxenFlacDecoder(const FoxenFlacDecoder&) = delete;
  FoxenFlacDecoder& operator=(const FoxenFlacDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  SourceBuffer buffer_;

  fx_flac_t* flac_;
};

}  // namespace codecs
