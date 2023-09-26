/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "opus.hpp"

#include <stdint.h>
#include <sys/_stdint.h>
#include <sys/unistd.h>

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

static int read_cb(void* src, unsigned char* ptr, int nbytes) {
  IStream* source = reinterpret_cast<IStream*>(src);
  return source->Read(
      {reinterpret_cast<std::byte*>(ptr), static_cast<size_t>(nbytes)});
}

static int seek_cb(void* src, int64_t offset, int whence) {
  IStream* source = reinterpret_cast<IStream*>(src);
  if (!source->CanSeek()) {
    return -1;
  }
  IStream::SeekFrom from;
  switch (whence) {
    case SEEK_CUR:
      from = IStream::SeekFrom::kCurrentPosition;
      break;
    case SEEK_END:
      from = IStream::SeekFrom::kEndOfStream;
      break;
    case SEEK_SET:
      from = IStream::SeekFrom::kStartOfStream;
      break;
    default:
      return -1;
  }
  source->SeekTo(offset, from);
  return 0;
}

static int64_t tell_cb(void* src) {
  IStream* source = reinterpret_cast<IStream*>(src);
  return source->CurrentPosition();
}

static const OpusFileCallbacks kCallbacks{
    .read = read_cb,
    .seek = seek_cb,
    .tell = tell_cb,
    .close = NULL,
};

XiphOpusDecoder::XiphOpusDecoder()
    : input_(nullptr), opus_(nullptr), num_channels_() {}

XiphOpusDecoder::~XiphOpusDecoder() {
  if (opus_ != nullptr) {
    op_free(opus_);
  }
}

auto XiphOpusDecoder::OpenStream(std::shared_ptr<IStream> input)
    -> cpp::result<OutputFormat, Error> {
  input_ = input;

  int res;
  opus_ = op_open_callbacks(input.get(), &kCallbacks, nullptr, 0, &res);

  if (res < 0) {
    std::pmr::string err;
    switch (res) {
      case OP_EREAD:
        err = "OP_EREAD";
        break;
      case OP_EFAULT:
        err = "OP_EFAULT";
        break;
      case OP_EIMPL:
        err = "OP_EIMPL";
        break;
      case OP_EINVAL:
        err = "OP_EINVAL";
        break;
      case OP_ENOTFORMAT:
        err = "OP_ENOTFORMAT";
        break;
      case OP_EBADHEADER:
        err = "OP_EBADHEADER";
        break;
      case OP_EVERSION:
        err = "OP_EVERSION";
        break;
      case OP_EBADLINK:
        err = "OP_EBADLINK";
        break;
      case OP_EBADTIMESTAMP:
        err = "OP_BADTIMESTAMP";
        break;
      default:
        err = "unknown";
    }
    ESP_LOGE(kTag, "error beginning stream: %s", err.c_str());
    return cpp::fail(Error::kMalformedData);
  }

  auto l = op_pcm_total(opus_, -1);
  std::optional<uint32_t> length;
  if (l > 0) {
    length = l * 2;
  }

  return OutputFormat{
      .num_channels = 2,
      .sample_rate_hz = 48000,
      .total_samples = length,
  };
}

auto XiphOpusDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  int samples_written = op_read_stereo(opus_, output.data(), output.size());

  if (samples_written < 0) {
    ESP_LOGE(kTag, "read failed %i", samples_written);
    return cpp::fail(Error::kMalformedData);
  }

  samples_written *= 2;  // Fixed to stereo
  return OutputInfo{
      .samples_written = static_cast<size_t>(samples_written),
      .is_stream_finished = samples_written == 0,
  };
}

auto XiphOpusDecoder::SeekTo(size_t target) -> cpp::result<void, Error> {
  if (op_pcm_seek(opus_, target) != 0) {
    return cpp::fail(Error::kInternalError);
  }
  return {};
}

}  // namespace codecs
