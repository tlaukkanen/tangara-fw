/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "native.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

#include "esp_heap_caps.h"
#include "mad.h"

#include "codec.hpp"
#include "esp_log.h"
#include "result.hpp"
#include "sample.hpp"
#include "types.hpp"

namespace codecs {

NativeDecoder::NativeDecoder() : input_() {}

auto NativeDecoder::OpenStream(std::shared_ptr<IStream> input, uint32_t offset)
    -> cpp::result<OutputFormat, ICodec::Error> {
  input_ = input;
  return OutputFormat{
      .num_channels = 1,
      .sample_rate_hz = 48000,
      .total_samples = {},
  };
}

auto NativeDecoder::DecodeTo(std::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  size_t bytes = input_->Read({
      reinterpret_cast<std::byte*>(output.data()),
      output.size_bytes(),
  });
  return OutputInfo{
      .samples_written = bytes / sizeof(sample::Sample),
      .is_stream_finished = false,
  };
}

}  // namespace codecs
