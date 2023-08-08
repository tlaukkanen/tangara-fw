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
    : flac_(FX_FLAC_ALLOC(FLAC_MAX_BLOCK_SIZE, 2)) {}

FoxenFlacDecoder::~FoxenFlacDecoder() {
  free(flac_);
}

auto FoxenFlacDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  uint32_t bytes_used = input.size_bytes();
  fx_flac_state_t state =
      fx_flac_process(flac_, reinterpret_cast<const uint8_t*>(input.data()),
                      &bytes_used, NULL, NULL);
  if (state != FLAC_END_OF_METADATA) {
    if (state == FLAC_ERR) {
      return {bytes_used, cpp::fail(Error::kMalformedData)};
    } else {
      return {bytes_used, cpp::fail(Error::kOutOfInput)};
    }
  }

  int64_t channels = fx_flac_get_streaminfo(flac_, FLAC_KEY_N_CHANNELS);
  int64_t fs = fx_flac_get_streaminfo(flac_, FLAC_KEY_SAMPLE_RATE);
  if (channels == FLAC_INVALID_METADATA_KEY ||
      fs == FLAC_INVALID_METADATA_KEY) {
    return {bytes_used, cpp::fail(Error::kMalformedData)};
  }

  OutputFormat format{
      .num_channels = static_cast<uint8_t>(channels),
      .sample_rate_hz = static_cast<uint32_t>(fs),
      .duration_seconds = {},
      .bits_per_second = {},
  };

  uint64_t num_samples = fx_flac_get_streaminfo(flac_, FLAC_KEY_N_SAMPLES);
  if (num_samples > 0) {
    format.duration_seconds = num_samples / fs;
  }

  return {bytes_used, format};
}

auto FoxenFlacDecoder::ContinueStream(cpp::span<const std::byte> input,
                                      cpp::span<sample::Sample> output)
    -> Result<OutputInfo> {
  cpp::span<int32_t> output_as_samples{
      reinterpret_cast<int32_t*>(output.data()), output.size_bytes() / 4};
  uint32_t bytes_read = input.size_bytes();
  uint32_t samples_written = output_as_samples.size();

  fx_flac_state_t state =
      fx_flac_process(flac_, reinterpret_cast<const uint8_t*>(input.data()),
                      &bytes_read, output_as_samples.data(), &samples_written);
  if (state == FLAC_ERR) {
    return {bytes_read, cpp::fail(Error::kMalformedData)};
  }

  if (samples_written > 0) {
    return {bytes_read,
            OutputInfo{.samples_written = samples_written,
                       .is_finished_writing = state == FLAC_END_OF_FRAME}};
  }

  // No error, but no samples written. We must be out of data.
  return {bytes_read, cpp::fail(Error::kOutOfInput)};
}

auto FoxenFlacDecoder::SeekStream(cpp::span<const std::byte> input,
                                  std::size_t target_sample) -> Result<void> {
  // TODO(jacqueline): Implement me.
  return {0, {}};
}

}  // namespace codecs
