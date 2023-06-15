/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "stbvorbis.hpp"
#include <stdint.h>

#include <cstdint>
#include <optional>

#include "stb_vorbis.h"

namespace codecs {

StbVorbisDecoder::StbVorbisDecoder()
    : vorbis_(nullptr),
      current_sample_(-1),
      num_channels_(0),
      num_samples_(0),
      samples_array_(NULL) {}

StbVorbisDecoder::~StbVorbisDecoder() {
  if (vorbis_ != nullptr) {
    stb_vorbis_close(vorbis_);
  }
}

static uint32_t scaleToBits(float sample, uint8_t bits) {
  // Scale to range.
  int32_t max_val = (1 << (bits - 1));
  int32_t fixed_point = sample * max_val;

  // Clamp within bounds.
  fixed_point = std::clamp(fixed_point, -max_val, max_val);

  // Remove sign.
  return *reinterpret_cast<uint32_t*>(&fixed_point);
}

auto StbVorbisDecoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  if (vorbis_ != nullptr) {
    stb_vorbis_close(vorbis_);
    vorbis_ = nullptr;
  }
  current_sample_ = -1;
  int bytes_read = 0;
  int error = 0;
  vorbis_ =
      stb_vorbis_open_pushdata(reinterpret_cast<const uint8_t*>(input.data()),
                               input.size_bytes(), &bytes_read, &error, NULL);
  if (error != 0) {
    return {0, cpp::fail(Error::kMalformedData)};
  }
  stb_vorbis_info info = stb_vorbis_get_info(vorbis_);
  return {bytes_read,
          OutputFormat{.num_channels = static_cast<uint8_t>(info.channels),
                       .bits_per_sample = 24,
                       .sample_rate_hz = info.sample_rate}};
}

auto StbVorbisDecoder::ContinueStream(cpp::span<const std::byte> input,
                                      cpp::span<std::byte> output)
    -> Result<OutputInfo> {
  std::size_t bytes_used = 0;
  if (current_sample_ < 0) {
    num_channels_ = 0;
    num_samples_ = 0;
    samples_array_ = NULL;

    while (true) {
      auto cropped = input.subspan(bytes_used);
      std::size_t b = stb_vorbis_decode_frame_pushdata(
          vorbis_, reinterpret_cast<const uint8_t*>(cropped.data()),
          cropped.size_bytes(), &num_channels_, &samples_array_, &num_samples_);
      if (b == 0) {
        return {bytes_used, cpp::fail(Error::kOutOfInput)};
      }
      bytes_used += b;

      if (num_samples_ == 0) {
        // Decoder is synchronising. Decode more bytes.
        continue;
      }
      if (num_channels_ == 0 || samples_array_ == NULL) {
        // The decoder isn't satisfying its contract.
        return {bytes_used, cpp::fail(Error::kInternalError)};
      }
      current_sample_ = 0;
      break;
    }
  }

  // We successfully decoded a frame. Time to write out the samples.
  std::size_t output_byte = 0;
  while (current_sample_ < num_samples_) {
    if (output_byte + (2 * num_channels_) >= output.size()) {
      return {0, OutputInfo{.bytes_written = output_byte,
                            .is_finished_writing = false}};
    }

    for (int channel = 0; channel < num_channels_; channel++) {
      float raw_sample = samples_array_[channel][current_sample_];

      uint16_t sample_24 = scaleToBits(raw_sample, 24);
      output[output_byte++] = static_cast<std::byte>((sample_24 >> 16) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_24 >> 8) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_24)&0xFF);
      // Pad to 32 bits for alignment.
      output[output_byte++] = static_cast<std::byte>(0);
    }
    current_sample_++;
  }

  current_sample_ = -1;
  return {bytes_used, OutputInfo{.bytes_written = output_byte,
                                 .is_finished_writing = true}};
}

auto StbVorbisDecoder::SeekStream(cpp::span<const std::byte> input,
                                  std::size_t target_sample) -> Result<void> {
  // TODO(jacqueline): Implement me.
  return {0, {}};
}

}  // namespace codecs
