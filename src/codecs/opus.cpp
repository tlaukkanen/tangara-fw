/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "opus.hpp"
#include <stdint.h>
#include <sys/_stdint.h>

#include <cstdint>
#include <cstring>
#include <optional>

#include "mad.h"

#include "codec.hpp"
#include "esp_log.h"
#include "opus.h"
#include "result.hpp"
#include "types.hpp"

namespace codecs {

  static constexpr std::size_t kSampleBufferSize = 5760 * sizeof(float);

XiphOpusDecoder::XiphOpusDecoder() {
  int err;
  opus_ = opus_decoder_create(48000, 2, &err);
  assert(err == OPUS_OK);
}
XiphOpusDecoder::~XiphOpusDecoder() {
  opus_decoder_destroy(opus_);
}

auto XiphOpusDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {}

auto XiphOpusDecoder::ContinueStream(cpp::span<const std::byte> input,
                                     cpp::span<std::byte> output)
    -> Result<OutputInfo> {
  int samples_decoded = opus_decode_float(
      opus_, reinterpret_cast<const unsigned char*>(input.data()),
      input.size_bytes(), sample_buffer_, sample_buffer_len_, 0);
}

auto XiphOpusDecoder::SeekStream(cpp::span<const std::byte> input,
                                 std::size_t target_sample) -> Result<void> {
  return {};
}

}  // namespace codecs
