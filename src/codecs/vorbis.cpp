/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ivorbiscodec.h"
#include "ivorbisfile.h"
#include "ogg/config_types.h"
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
#include "vorbis.hpp"

namespace codecs {

static constexpr char kTag[] = "vorbis";

size_t read_cb(void* ptr, size_t size, size_t nmemb, void* instance) {
  TremorVorbisDecoder* dec = reinterpret_cast<TremorVorbisDecoder*>(instance);
  auto input = dec->ReadCallback();
  size_t amount_to_read = std::min<size_t>(size * nmemb, input.size_bytes());
  std::memcpy(ptr, input.data(), amount_to_read);
  dec->AfterReadCallback(amount_to_read);
  return amount_to_read;
}

int seek_cb(void* instance, ogg_int64_t offset, int whence) {
  // Seeking is handled separately.
  return -1;
}

int close_cb(void* instance) {
  return 0;
}

static const ov_callbacks kCallbacks{
    .read_func = read_cb,
    .seek_func = seek_cb,
    .close_func = close_cb,
    .tell_func = NULL,  // Not seekable
};

TremorVorbisDecoder::TremorVorbisDecoder()
    : vorbis_(), input_(), pos_in_input_(0) {}

TremorVorbisDecoder::~TremorVorbisDecoder() {
  ov_clear(&vorbis_);
}

auto TremorVorbisDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  int res = ov_open_callbacks(this, &vorbis_,
                              reinterpret_cast<const char*>(input.data()),
                              input.size(), kCallbacks);
  if (res < 0) {
    std::string err;
    switch (res) {
      case OV_EREAD:
        err = "OV_EREAD";
        break;
      case OV_ENOTVORBIS:
        err = "OV_ENOTVORBIS";
        break;
      case OV_EVERSION:
        err = "OV_EVERSION";
        break;
      case OV_EBADHEADER:
        err = "OV_EBADHEADER";
        break;
      case OV_EFAULT:
        err = "OV_EFAULT";
        break;
      default:
        err = "unknown";
    }
    ESP_LOGE(kTag, "error beginning stream: %s", err.c_str());
    return {input.size(), cpp::fail(Error::kMalformedData)};
  }

  vorbis_info* info = ov_info(&vorbis_, -1);
  if (info == NULL) {
    ESP_LOGE(kTag, "failed to get stream info");
    return {input.size(), cpp::fail(Error::kMalformedData)};
  }

  return {input.size(),
          OutputFormat{
              .num_channels = static_cast<uint8_t>(info->channels),
              .sample_rate_hz = static_cast<uint32_t>(info->rate),
              .bits_per_second = info->bitrate_nominal,
          }};
}

auto TremorVorbisDecoder::ContinueStream(cpp::span<const std::byte> input,
                                         cpp::span<sample::Sample> output)
    -> Result<OutputInfo> {
  cpp::span<int16_t> staging_buffer{
      reinterpret_cast<int16_t*>(output.subspan(output.size() / 2).data()),
      output.size_bytes() / 2};

  input_ = input;
  pos_in_input_ = 0;

  int bitstream;
  long bytes_written =
      ov_read(&vorbis_, reinterpret_cast<char*>(staging_buffer.data()),
              staging_buffer.size_bytes(), &bitstream);
  if (bytes_written == OV_HOLE) {
    ESP_LOGE(kTag, "got OV_HOLE");
    return {pos_in_input_, cpp::fail(Error::kMalformedData)};
  } else if (bytes_written == OV_EBADLINK) {
    ESP_LOGE(kTag, "got OV_EBADLINK");
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

auto TremorVorbisDecoder::SeekStream(cpp::span<const std::byte> input,
                                     std::size_t target_sample)
    -> Result<void> {
  return {};
}

auto TremorVorbisDecoder::ReadCallback() -> cpp::span<const std::byte> {
  return input_.subspan(pos_in_input_);
}

auto TremorVorbisDecoder::AfterReadCallback(size_t bytes_read) -> void {
  pos_in_input_ += bytes_read;
}

}  // namespace codecs
