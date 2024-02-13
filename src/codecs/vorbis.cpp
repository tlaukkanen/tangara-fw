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

[[maybe_unused]] static constexpr char kTag[] = "vorbis";

static size_t read_cb(void* ptr, size_t size, size_t nmemb, void* instance) {
  IStream* source = reinterpret_cast<IStream*>(instance);
  return source->Read({reinterpret_cast<std::byte*>(ptr), size * nmemb});
}

static int seek_cb(void* instance, ogg_int64_t offset, int whence) {
  IStream* source = reinterpret_cast<IStream*>(instance);
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

static int close_cb(void* src) {
  return 0;
}

static long tell_cb(void* src) {
  IStream* source = reinterpret_cast<IStream*>(src);
  return source->CurrentPosition();
}

static const ov_callbacks kCallbacks{
    .read_func = read_cb,
    .seek_func = seek_cb,
    .close_func = close_cb,
    .tell_func = tell_cb,  // Not seekable
};

TremorVorbisDecoder::TremorVorbisDecoder()
    : input_(),
      vorbis_(reinterpret_cast<OggVorbis_File*>(
          heap_caps_malloc(sizeof(OggVorbis_File),
                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))) {}

TremorVorbisDecoder::~TremorVorbisDecoder() {
  ov_clear(vorbis_.get());
}

auto TremorVorbisDecoder::OpenStream(std::shared_ptr<IStream> input)
    -> cpp::result<OutputFormat, Error> {
  int res = ov_open_callbacks(input.get(), vorbis_.get(), NULL, 0, kCallbacks);
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
    return cpp::fail(Error::kMalformedData);
  }

  vorbis_info* info = ov_info(vorbis_.get(), -1);
  if (info == NULL) {
    ESP_LOGE(kTag, "failed to get stream info");
    return cpp::fail(Error::kMalformedData);
  }

  auto l = ov_pcm_total(vorbis_.get(), -1);
  std::optional<uint32_t> length;
  if (l > 0) {
    length = l * info->channels;
  }

  return OutputFormat{
      .num_channels = static_cast<uint8_t>(info->channels),
      .sample_rate_hz = static_cast<uint32_t>(info->rate),
      .total_samples = length,
  };
}

auto TremorVorbisDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  int unused = 0;
  long bytes_written =
      ov_read(vorbis_.get(), reinterpret_cast<char*>(output.data()),
              ((output.size() - 1) * sizeof(sample::Sample)), &unused);
  if (bytes_written == OV_HOLE) {
    ESP_LOGE(kTag, "got OV_HOLE");
    return cpp::fail(Error::kMalformedData);
  } else if (bytes_written == OV_EBADLINK) {
    ESP_LOGE(kTag, "got OV_EBADLINK");
    return cpp::fail(Error::kMalformedData);
  }

  return OutputInfo{
      .samples_written =
          static_cast<size_t>(bytes_written / sizeof(sample::Sample)),
      .is_stream_finished = bytes_written == 0,
  };
}

auto TremorVorbisDecoder::SeekTo(size_t target) -> cpp::result<void, Error> {
  if (ov_pcm_seek(vorbis_.get(), target) != 0) {
    return cpp::fail(Error::kInternalError);
  }
  return {};
}

}  // namespace codecs
