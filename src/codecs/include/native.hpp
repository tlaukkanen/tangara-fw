/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "mad.h"
#include "sample.hpp"
#include "source_buffer.hpp"

#include "codec.hpp"

namespace codecs {

class NativeDecoder : public ICodec {
 public:
  NativeDecoder();

  auto OpenStream(std::shared_ptr<IStream> input, uint32_t offset)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(std::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  NativeDecoder(const NativeDecoder&) = delete;
  NativeDecoder& operator=(const NativeDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
};

}  // namespace codecs
