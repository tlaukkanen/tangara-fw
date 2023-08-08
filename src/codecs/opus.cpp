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

#include "esp_heap_caps.h"
#include "mad.h"

#include "codec.hpp"
#include "esp_log.h"
#include "opus.h"
#include "opus_types.h"
#include "result.hpp"
#include "sample.hpp"
#include "types.hpp"

namespace codecs {

static constexpr char kTag[] = "opus";

// "If this is less than the maximum packet duration (120ms; 5760 for 48kHz),
// this function will not be capable of decoding some packets"
static constexpr size_t kSampleBufferSize = 5760;

XiphOpusDecoder::XiphOpusDecoder() {
  int err;
  opus_ = opus_decoder_create(48000, 2, &err);
  assert(err == OPUS_OK);

  pos_in_buffer_ = 0;
  sample_buffer_ = {reinterpret_cast<opus_int16*>(
                        heap_caps_calloc(kSampleBufferSize, sizeof(opus_int16),
                                         MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)),
                    kSampleBufferSize};
}
XiphOpusDecoder::~XiphOpusDecoder() {
  opus_decoder_destroy(opus_);
  heap_caps_free(sample_buffer_.data());
}

auto XiphOpusDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  return {0, OutputFormat{
                 .num_channels = 2,
                 .sample_rate_hz = 48000,
             }};
}

auto read_uint32(cpp::span<const std::byte> src) -> uint32_t {
  return static_cast<uint32_t>(src[0] << 24) |
         static_cast<uint32_t>(src[1] << 16) |
         static_cast<uint32_t>(src[2] << 8) |
         static_cast<uint32_t>(src[3] << 0);
}

auto XiphOpusDecoder::ContinueStream(cpp::span<const std::byte> input,
                                     cpp::span<sample::Sample> output)
    -> Result<OutputInfo> {
  size_t bytes_used = 0;
  if (pos_in_buffer_ >= samples_in_buffer_) {
    ESP_LOGI(kTag, "sample buffer is empty. parsing more.");
    if (input.size() < 4) {
      return {0, cpp::fail(Error::kOutOfInput)};
    }
    uint32_t payload_length = read_uint32(input);
    ESP_LOGI(kTag, "payload length is %lu", payload_length);

    if (input.size() - 4 < payload_length) {
      ESP_LOGI(kTag, "input too small for payload");
      return {0, cpp::fail(Error::kOutOfInput)};
    }

    // Next 4 bytes are the 'final range'.
    // uint32_t enc_final_range = read_uint32(input.subspan(4));

    bytes_used = payload_length + 8;

    pos_in_buffer_ = 0;
    samples_in_buffer_ = opus_decode(
        opus_, reinterpret_cast<const unsigned char*>(input.data() + 8),
        payload_length, sample_buffer_.data(), sample_buffer_.size(), 0);

    if (samples_in_buffer_ < 0) {
      ESP_LOGE(kTag, "error decoding stream");
      return {bytes_used, cpp::fail(Error::kMalformedData)};
    }
  }

  size_t samples_written = 0;
  while (pos_in_buffer_ < samples_in_buffer_ &&
         samples_written < output.size()) {
    output[samples_written++] =
        sample::FromSigned(sample_buffer_[pos_in_buffer_++], 16);
  }

  return {bytes_used,
          OutputInfo{
              .samples_written = samples_written,
              .is_finished_writing = pos_in_buffer_ >= samples_in_buffer_,
          }};
}

auto XiphOpusDecoder::SeekStream(cpp::span<const std::byte> input,
                                 std::size_t target_sample) -> Result<void> {
  return {};
}

}  // namespace codecs
