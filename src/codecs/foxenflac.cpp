/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "foxenflac.hpp"
#include <stdint.h>
#include <sys/_stdint.h>

#include <cstdlib>

#include "esp_log.h"
#include "foxen/flac.h"
#include "sample.hpp"

namespace codecs {

static const char kTag[] = "flac";

FoxenFlacDecoder::FoxenFlacDecoder()
    : input_(), buffer_(), flac_(FX_FLAC_ALLOC(FLAC_MAX_BLOCK_SIZE, 2)) {}

FoxenFlacDecoder::~FoxenFlacDecoder() {
  free(flac_);
}

auto FoxenFlacDecoder::OpenStream(std::shared_ptr<IStream> input)
    -> cpp::result<OutputFormat, Error> {
  input_ = input;

  bool eof = false;
  fx_flac_state_t state;
  do {
    eof = buffer_.Refill(input_.get());
    buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
      uint32_t bytes_used = buf.size();
      state =
          fx_flac_process(flac_, reinterpret_cast<const uint8_t*>(buf.data()),
                          &bytes_used, NULL, NULL);
      return bytes_used;
    });
  } while (state != FLAC_END_OF_METADATA && !eof);

  if (state != FLAC_END_OF_METADATA) {
    if (state == FLAC_ERR) {
      return cpp::fail(Error::kMalformedData);
    } else {
      return cpp::fail(Error::kOutOfInput);
    }
  }

  int64_t channels = fx_flac_get_streaminfo(flac_, FLAC_KEY_N_CHANNELS);
  int64_t fs = fx_flac_get_streaminfo(flac_, FLAC_KEY_SAMPLE_RATE);
  if (channels == FLAC_INVALID_METADATA_KEY ||
      fs == FLAC_INVALID_METADATA_KEY) {
    return cpp::fail(Error::kMalformedData);
  }

  OutputFormat format{
      .num_channels = static_cast<uint8_t>(channels),
      .sample_rate_hz = static_cast<uint32_t>(fs),
  };

  uint64_t num_samples = fx_flac_get_streaminfo(flac_, FLAC_KEY_N_SAMPLES);
  if (num_samples > 0) {
    format.total_samples = num_samples * channels;
  }

  return format;
}

auto FoxenFlacDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  bool is_eof = buffer_.Refill(input_.get());

  cpp::span<int32_t> output32{reinterpret_cast<int32_t*>(output.data()),
                              output.size() / 2};
  uint32_t samples_written = output32.size();

  fx_flac_state_t state;
  buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_read = buf.size_bytes();
    state = fx_flac_process(flac_, reinterpret_cast<const uint8_t*>(buf.data()),
                            &bytes_read, output32.data(), &samples_written);
    return bytes_read;
  });
  if (state == FLAC_ERR) {
    return cpp::fail(Error::kMalformedData);
  }

  for (size_t i = 0; i < samples_written; i++) {
    output[i] = output32[i] >> 16;
  }

  return OutputInfo{.samples_written = samples_written,
                    .is_stream_finished = samples_written == 0 && is_eof};
}

auto FoxenFlacDecoder::SeekTo(size_t target) -> cpp::result<void, Error> {
  return {};
}

}  // namespace codecs
