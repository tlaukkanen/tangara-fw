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
#include "ogg/ogg.h"
#include "opus.h"
#include "opus_defines.h"
#include "opus_types.h"
#include "result.hpp"
#include "sample.hpp"
#include "types.hpp"

namespace codecs {

static constexpr char kTag[] = "opus";

// "If this is less than the maximum packet duration (120ms; 5760 for 48kHz),
// this function will not be capable of decoding some packets"
static constexpr size_t kSampleBufferSize = 5760;

XiphOpusDecoder::XiphOpusDecoder() : opus_(nullptr) {
  pos_in_buffer_ = 0;
  sample_buffer_ = {reinterpret_cast<opus_int16*>(
                        heap_caps_calloc(kSampleBufferSize, sizeof(opus_int16),
                                         MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)),
                    kSampleBufferSize};
}
XiphOpusDecoder::~XiphOpusDecoder() {
  if (opus_ != nullptr) {
    opus_decoder_destroy(opus_);
  }
  heap_caps_free(sample_buffer_.data());
}

auto XiphOpusDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  if (!ogg_.AddBytes(input)) {
    ESP_LOGI(kTag, "need more input to begin");
    return {input.size(), cpp::fail(Error::kOutOfInput)};
  }
  auto packet = ogg_.Current();
  int num_channels = opus_packet_get_nb_channels(packet.data());
  ESP_LOGI(kTag, "opus stream has %i channels", num_channels);
  if (num_channels > 2) {
    // Too many channels; we can't handle this.
    // TODO: better error
    return {input.size(), cpp::fail(Error::kMalformedData)};
  }

  int err;
  opus_ = opus_decoder_create(48000, num_channels, &err);
  if (err != OPUS_OK) {
    return {input.size(), cpp::fail(Error::kInternalError)};
  }

  return {input.size(), OutputFormat{
                            .num_channels = static_cast<uint8_t>(num_channels),
                            .sample_rate_hz = 48000,
                        }};
}

auto XiphOpusDecoder::ContinueStream(cpp::span<const std::byte> input,
                                     cpp::span<sample::Sample> output)
    -> Result<OutputInfo> {
  size_t bytes_used = 0;
  if (pos_in_buffer_ >= samples_in_buffer_) {
    if (!ogg_.HasPacket()) {
      bytes_used = input.size();
      if (!ogg_.AddBytes(input)) {
        return {bytes_used, cpp::fail(Error::kOutOfInput)};
      }
    }

    auto packet = ogg_.Current();
    pos_in_buffer_ = 0;
    samples_in_buffer_ = 0;
    while (samples_in_buffer_ <= 0 && ogg_.HasPacket()) {
      samples_in_buffer_ =
          opus_decode(opus_, packet.data(), packet.size_bytes(),
                      sample_buffer_.data(), sample_buffer_.size(), 0);
      ogg_.Next();
    }

    if (samples_in_buffer_ < 0) {
      std::string err_str;
      switch (samples_in_buffer_) {
        case OPUS_BAD_ARG:
          err_str = "OPUS_BAD_ARG";
          break;
        case OPUS_BUFFER_TOO_SMALL:
          err_str = "OPUS_BUFFER_TOO_SMALL";
          break;
        case OPUS_INTERNAL_ERROR:
          err_str = "OPUS_INTERNAL_ERROR";
          break;
        case OPUS_INVALID_PACKET:
          err_str = "OPUS_INVALID_PACKET";
          break;
        case OPUS_UNIMPLEMENTED:
          err_str = "OPUS_UNIMPLEMENTED";
          break;
        case OPUS_INVALID_STATE:
          err_str = "OPUS_INVALID_STATE";
          break;
        case OPUS_ALLOC_FAIL:
          err_str = "OPUS_ALLOC_FAIL";
          break;
        default:
          err_str = "unknown";
      }
      ESP_LOGE(kTag, "error decoding stream, err %s", err_str.c_str());
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
