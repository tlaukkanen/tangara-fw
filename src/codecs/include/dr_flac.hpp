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

#include "dr_flac.h"
#include "sample.hpp"
#include "source_buffer.hpp"

#include "codec.hpp"

namespace codecs {

class DrFlacDecoder : public ICodec {
 public:
  DrFlacDecoder();
  ~DrFlacDecoder();

  auto OpenStream(std::shared_ptr<IStream> input, uint32_t offset)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(std::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  DrFlacDecoder(const DrFlacDecoder&) = delete;
  DrFlacDecoder& operator=(const DrFlacDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  drflac* flac_;
};

}  // namespace codecs
