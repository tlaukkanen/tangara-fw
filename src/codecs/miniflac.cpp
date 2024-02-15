/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "miniflac.hpp"

#include <cstdint>
#include <cstdlib>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "miniflac.h"
#include "result.hpp"
#include "sample.hpp"

namespace codecs {

[[maybe_unused]] static const char kTag[] = "flac";

static constexpr size_t kMaxFrameSize = 4608;

MiniFlacDecoder::MiniFlacDecoder()
    : input_(),
      buffer_(),
      flac_(reinterpret_cast<miniflac_t*>(
          heap_caps_malloc(sizeof(miniflac_t),
                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))),
      current_sample_() {
  miniflac_init(flac_.get(), MINIFLAC_CONTAINER_UNKNOWN);
  for (int i = 0; i < samples_by_channel_.size(); i++) {
    // Full decoded frames too big to fit in internal ram :(
    samples_by_channel_[i] = reinterpret_cast<int32_t*>(
        heap_caps_malloc(kMaxFrameSize * sizeof(int32_t), MALLOC_CAP_SPIRAM));
  }
}

MiniFlacDecoder::~MiniFlacDecoder() {
  for (int i = 0; i < samples_by_channel_.size(); i++) {
    heap_caps_free(samples_by_channel_[i]);
  }
}

auto MiniFlacDecoder::OpenStream(std::shared_ptr<IStream> input,uint32_t offset)
    -> cpp::result<OutputFormat, Error> {
  input_ = input;

  MINIFLAC_RESULT res;
  auto read_until_result = [&](auto fn) {
    while (true) {
      bool eof = buffer_.Refill(input_.get());
      buffer_.ConsumeBytes(fn);
      if (res == MINIFLAC_CONTINUE && !eof) {
        continue;
      }
      break;
    }
  };

  uint32_t sample_rate = 0;

  read_until_result([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_used = 0;
    res = miniflac_streaminfo_sample_rate(
        flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
        buf.size_bytes(), &bytes_used, &sample_rate);
    return bytes_used;
  });

  if (res != MINIFLAC_OK) {
    return cpp::fail(Error::kMalformedData);
  }

  uint8_t channels = 0;

  read_until_result([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_used = 0;
    res = miniflac_streaminfo_channels(
        flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
        buf.size_bytes(), &bytes_used, &channels);
    return bytes_used;
  });

  if (res != MINIFLAC_OK) {
    return cpp::fail(Error::kMalformedData);
  }

  uint64_t total_samples = 0;

  read_until_result([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_used = 0;
    res = miniflac_streaminfo_total_samples(
        flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
        buf.size_bytes(), &bytes_used, &total_samples);
    return bytes_used;
  });

  if (res != MINIFLAC_OK) {
    return cpp::fail(Error::kMalformedData);
  }

  if (channels == 0 || channels > 2) {
    return cpp::fail(Error::kMalformedData);
  }

  OutputFormat format{
      .num_channels = static_cast<uint8_t>(channels),
      .sample_rate_hz = static_cast<uint32_t>(sample_rate),
      .total_samples = total_samples * channels,
  };

  return format;
}

auto MiniFlacDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  bool is_eof = false;

  if (!current_sample_) {
    MINIFLAC_RESULT res = MINIFLAC_CONTINUE;
    while (res == MINIFLAC_CONTINUE && !is_eof) {
      is_eof = buffer_.Refill(input_.get());
      buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
        // FIXME: We should do a miniflac_sync first, in order to check that
        // our sample buffers have enough space for the next frame.
        uint32_t bytes_read = 0;
        res = miniflac_decode(
            flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
            buf.size_bytes(), &bytes_read, samples_by_channel_.data());
        return bytes_read;
      });
    }

    if (res == MINIFLAC_OK) {
      current_sample_ = 0;
    } else if (is_eof) {
      return OutputInfo{
          .samples_written = 0,
          .is_stream_finished = true,
      };
    } else {
      return cpp::fail(Error::kMalformedData);
    }
  }

  size_t samples_written = 0;
  if (current_sample_) {
    while (*current_sample_ < flac_->frame.header.block_size) {
      if (samples_written + flac_->frame.header.channels >= output.size()) {
        // We can't fit the next full PCM frame into the buffer.
        return OutputInfo{.samples_written = samples_written,
                          .is_stream_finished = false};
      }

      for (int channel = 0; channel < flac_->frame.header.channels; channel++) {
        output[samples_written++] =
            sample::FromSigned(samples_by_channel_[channel][*current_sample_],
                               flac_->frame.header.bps);
      }
      (*current_sample_)++;
    }
  }

  current_sample_.reset();
  return OutputInfo{.samples_written = samples_written,
                    .is_stream_finished = samples_written == 0 && is_eof};
}

auto MiniFlacDecoder::SeekTo(size_t target) -> cpp::result<void, Error> {
  return {};
}

}  // namespace codecs
