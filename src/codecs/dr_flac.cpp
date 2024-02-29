/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dr_flac.hpp"

#include <cstdint>
#include <cstdlib>

#include "codec.hpp"
#include "dr_flac.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "result.hpp"
#include "sample.hpp"

namespace codecs {

[[maybe_unused]] static const char kTag[] = "flac";

static void* onMalloc(size_t sz, void* pUserData) {
  return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
}

static void* onRealloc(void* p, size_t sz, void* pUserData) {
  return heap_caps_realloc(p, sz, MALLOC_CAP_SPIRAM);
}

static void onFree(void* p, void* pUserData) {
  heap_caps_free(p);
}

static drflac_allocation_callbacks kAllocCallbacks{
    .pUserData = nullptr,
    .onMalloc = onMalloc,
    .onRealloc = onRealloc,
    .onFree = onFree,
};

static size_t readProc(void* pUserData, void* pBufferOut, size_t bytesToRead) {
  IStream* stream = reinterpret_cast<IStream*>(pUserData);
  ssize_t res =
      stream->Read({reinterpret_cast<std::byte*>(pBufferOut), bytesToRead});
  return res < 0 ? 0 : res;
}

static drflac_bool32 seekProc(void* pUserData,
                              int offset,
                              drflac_seek_origin origin) {
  IStream* stream = reinterpret_cast<IStream*>(pUserData);
  if (!stream->CanSeek()) {
    return DRFLAC_FALSE;
  }

  IStream::SeekFrom seek_from;
  switch (origin) {
    case drflac_seek_origin_start:
      seek_from = IStream::SeekFrom::kStartOfStream;
      break;
    case drflac_seek_origin_current:
      seek_from = IStream::SeekFrom::kCurrentPosition;
      break;
    default:
      return DRFLAC_FALSE;
  }
  stream->SeekTo(offset, seek_from);

  // FIXME: Detect falling off the end of the file.
  return DRFLAC_TRUE;
}

DrFlacDecoder::DrFlacDecoder() : input_(), flac_() {}

DrFlacDecoder::~DrFlacDecoder() {
  if (flac_) {
    drflac_free(flac_, &kAllocCallbacks);
  }
}

auto DrFlacDecoder::OpenStream(std::shared_ptr<IStream> input, uint32_t offset)
    -> cpp::result<OutputFormat, Error> {
  input_ = input;

  flac_ = drflac_open(readProc, seekProc, input_.get(), &kAllocCallbacks);
  if (!flac_) {
    return cpp::fail(Error::kMalformedData);
  }

  if (offset && !drflac_seek_to_pcm_frame(flac_, offset * flac_->sampleRate)) {
    return cpp::fail(Error::kMalformedData);
  }

  OutputFormat format{
      .num_channels = static_cast<uint8_t>(flac_->channels),
      .sample_rate_hz = static_cast<uint32_t>(flac_->sampleRate),
      .total_samples = flac_->totalPCMFrameCount * flac_->channels,
  };
  return format;
}

auto DrFlacDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  size_t frames_to_read = output.size() / flac_->channels / 2;

  auto frames_written = drflac_read_pcm_frames_s16(
      flac_, output.size() / flac_->channels, output.data());

  return OutputInfo{
      .samples_written = static_cast<size_t>(frames_written * flac_->channels),
      .is_stream_finished = frames_written < frames_to_read};
}

auto DrFlacDecoder::SeekTo(size_t target) -> cpp::result<void, Error> {
  return {};
}

}  // namespace codecs
