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
#include "opusfile.h"
#include "result.hpp"
#include "sample.hpp"
#include "types.hpp"

namespace codecs {

static constexpr char kTag[] = "opus";

int read_cb(void* instance, unsigned char* ptr, int nbytes) {
  XiphOpusDecoder* dec = reinterpret_cast<XiphOpusDecoder*>(instance);
  auto input = dec->ReadCallback();
  size_t amount_to_read = std::min<size_t>(nbytes, input.size_bytes());
  std::memcpy(ptr, input.data(), amount_to_read);
  dec->AfterReadCallback(amount_to_read);
  return amount_to_read;
}

static const OpusFileCallbacks kCallbacks{
    .read = read_cb,
    .seek = NULL,
    .tell = NULL,  // Not seekable
    .close = NULL,
};

XiphOpusDecoder::XiphOpusDecoder() : opus_(nullptr) {}

XiphOpusDecoder::~XiphOpusDecoder() {
  if (opus_ != nullptr) {
    op_free(opus_);
  }
}

auto XiphOpusDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  int res;
  opus_ = op_open_callbacks(
      this, &kCallbacks, reinterpret_cast<const unsigned char*>(input.data()),
      input.size(), &res);

  if (res < 0) {
    std::string err;
    switch (res) {
      case OP_EREAD:
        err = "OP_EREAD";
        break;
      default:
        err = "unknown";
    }
    ESP_LOGE(kTag, "error beginning stream: %s", err.c_str());
    return {input.size(), cpp::fail(Error::kMalformedData)};
  }

  return {input.size(), OutputFormat{
                            .num_channels = 2,
                            .sample_rate_hz = 48000,
                        }};
}

auto XiphOpusDecoder::ContinueStream(cpp::span<const std::byte> input,
                                     cpp::span<sample::Sample> output)
    -> Result<OutputInfo> {
  cpp::span<int16_t> staging_buffer{
      reinterpret_cast<int16_t*>(output.subspan(output.size() / 2).data()),
      output.size_bytes() / 2};

  input_ = input;
  pos_in_input_ = 0;

  int bytes_written =
      op_read_stereo(opus_, staging_buffer.data(), staging_buffer.size());
  if (bytes_written < 0) {
    ESP_LOGE(kTag, "read failed %i", bytes_written);
    return {pos_in_input_, cpp::fail(Error::kMalformedData)};
  } else if (bytes_written == 0) {
    return {pos_in_input_, cpp::fail(Error::kOutOfInput)};
  }

  for (int i = 0; i < bytes_written / 2; i++) {
    output[i] = sample::FromSigned(staging_buffer[i], 16);
  }

  return {pos_in_input_,
          OutputInfo{
              .samples_written = static_cast<size_t>(bytes_written / 2),
              .is_finished_writing = bytes_written == 0,
          }};
}

auto XiphOpusDecoder::SeekStream(cpp::span<const std::byte> input,
                                 std::size_t target_sample) -> Result<void> {
  return {};
}

auto XiphOpusDecoder::ReadCallback() -> cpp::span<const std::byte> {
  return input_.subspan(pos_in_input_);
}

auto XiphOpusDecoder::AfterReadCallback(size_t bytes_read) -> void {
  pos_in_input_ += bytes_read;
}

}  // namespace codecs
