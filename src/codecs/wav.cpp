/*
 * Copyright 2023 Daniel <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "wav.hpp"
#include <stdint.h>
#include <sys/_stdint.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include "debug.hpp"
#include "esp_log.h"
#include "sample.hpp"

namespace codecs {

[[maybe_unused]] static const char kTag[] = "wav";

static inline auto bytes_to_u16(cpp::span<std::byte const, 2> bytes)
    -> uint16_t {
  return (uint16_t)bytes[0] | (uint16_t)bytes[1] << 8;
}

static inline auto bytes_to_u32(cpp::span<std::byte const, 4> bytes)
    -> uint32_t {
  return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
         (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static inline auto bytes_to_str(cpp::span<std::byte const> bytes)
    -> std::string {
  return std::string(reinterpret_cast<const char*>(bytes.data()),
                    bytes.size_bytes());
}

static int16_t convert_f32_to_16_bit(cpp::span<const std::byte> bytes) {
  uint64_t val = 0;
  val = (uint8_t)bytes[3];
  val = (val << 8) | (uint8_t)bytes[2];
  val = (val << 8) | (uint8_t)bytes[1];
  val = (val << 8) | (uint8_t)bytes[0];
  // Isolate the sign and remove from the value
  uint64_t sign = val >> 31;
  val -= (sign << 31);
  // Isolate the exponent and remove from the value
  uint64_t exp = (val >> 23);
  val -= (exp << 23);
  // Remove old bias and add new bias
  exp = exp - 127 + 1023;
  // Reconstruct the bits in the correct order and convert to double
  uint64_t dval = (sign << 63) + (exp << 52) + (val << 29);
  double* fval = reinterpret_cast<double*>(&dval);
  return sample::FromDouble(*fval);
}

static int16_t convert_to_16_bit(cpp::span<const std::byte> bytes) {
  int depth = bytes.size();
  int32_t val = 0;
  // If 8-bit Assume Unsigned
  if (depth == 1) {
    return sample::FromUnsigned((uint8_t)bytes[0], 8);
  }
  // Otherwise, build the signed int of the right depth
  switch (depth) {
    case 4:
      val = (uint8_t)bytes[3];
    case 3:
      val = (val << 8) | (uint8_t)bytes[2];
    case 2:
      val = (val << 8) | (uint8_t)bytes[1];
    case 1:
      val = (val << 8) | (uint8_t)bytes[0];
  }
  // Convert to sample
  int16_t result = sample::FromSigned(val, depth * 8);
  return result;
}

WavDecoder::WavDecoder() : input_(), buffer_() {}

WavDecoder::~WavDecoder() {}

auto WavDecoder::OpenStream(std::shared_ptr<IStream> input,uint32_t offset)
    -> cpp::result<OutputFormat, Error> {
  input_ = input;

  std::array<std::byte, 255> buf{std::byte{0}};
  auto size = input->Read(buf);
  if (size < 44) {
    return cpp::fail(Error::kOutOfInput);
  }

  // - check the first 4 bytes = 'RIFF'
  // - next 4 bytes = file size
  // - check next 4 bytes = 'WAVE'
  // - index of 'fmt\0' (i) marks start of fmt data
  // - i + 4 = size of fmt header (16, 18 or 40)
  // - i + 8 = format (should be 0x01 for pcm, 0xfffe for
  // wave_format_exstensible)
  // - i + 10 = num channels
  // - i + 12 = sample rate
  // - i + 16 = byte rate (sample rate * channels * bits per sample / 8)
  // - i + 20 = sample size (bits per sample * channels / 8)
  // - i + 22 = bits per sample (2 bytes)
  // - end of this part, next header we care about is 'data'
  // - and then the next 4 bytes = 32 bit int = size of data

  auto buffer_span = cpp::span{buf};

  std::string riff = bytes_to_str(buffer_span.subspan(0, 4));
  if (riff != "RIFF") {
    ESP_LOGW(kTag, "file is not RIFF");
    return cpp::fail(Error::kMalformedData);
  }

  uint32_t file_size = bytes_to_u32(buffer_span.subspan(4, 4)) + 8;

  std::string fmt_header = bytes_to_str(buffer_span.subspan(12, 4));
  ESP_LOGI(kTag, "fmt header found? %s",
           (fmt_header.starts_with("fmt")) ? "yes" : "no");
  if (!fmt_header.starts_with("fmt")) {
    ESP_LOGW(kTag, "Could not find format chunk");
    return cpp::fail(Error::kMalformedData);
  }

  // Size of the fmt header, should be 16, 18 or 40
  uint32_t fmt_header_size = bytes_to_u32(buffer_span.subspan(16, 4));

  wave_format_ = bytes_to_u16(buffer_span.subspan(20, 2));
  if (wave_format_ == kWaveFormatPCM) {
    ESP_LOGD(kTag, "wave format: PCM");
  } else if (wave_format_ == kWaveFormatExtensible) {
    ESP_LOGD(kTag, "wave format: extensible");
  } else if (wave_format_ == kWaveFormatIEEEFloat) {
    ESP_LOGD(kTag, "wave format: IEEE Float");
  } else {
    ESP_LOGW(kTag, "WAVE format not supported");
    return cpp::fail(Error::kUnsupportedFormat);
  }

  num_channels_ = bytes_to_u16(buffer_span.subspan(22, 2));

  uint32_t samples_per_second = bytes_to_u32(buffer_span.subspan(24, 4));

  uint32_t avg_bytes_per_second = bytes_to_u32(buffer_span.subspan(28, 4));

  uint16_t block_align = bytes_to_u16(buffer_span.subspan(32, 2));

  bytes_per_sample_ = block_align / num_channels_;

  uint16_t bits_per_sample = bytes_to_u16(buffer_span.subspan(34, 2));

  // find the start of the data chunk
  std::array<std::byte, 4> data_tag = {std::byte{0x64}, std::byte{0x61},
                                       std::byte{0x74}, std::byte{0x61}};
  auto data_loc = std::ranges::search(buffer_span, data_tag);
  if (data_loc.begin() == buffer_span.end()) {
    ESP_LOGW(kTag, "Could not find data chunk!");
    return cpp::fail(Error::kMalformedData);
  }

  int data_chunk_index = std::distance(buffer_span.begin(), data_loc.begin());

  uint32_t data_chunk_size =
      bytes_to_u32(buffer_span.subspan(data_chunk_index + 4, 4));

  // calculate number of samples
  int number_of_samples = data_chunk_size / bytes_per_sample_;

  // extension to the fmt chunk size (0 or 22)
  uint16_t extension_size = 0;
  if (wave_format_ == kWaveFormatExtensible) {
    extension_size = bytes_to_u16(buffer_span.subspan(36, 2));
  }

  // Parse extension if applicable
  if (extension_size == 22) {
    // Valid bits per sample
    uint16_t valid_bits_per_sample = bytes_to_u16(buffer_span.subspan(38, 2));

    uint32_t speaker_mask = bytes_to_u32(buffer_span.subspan(40, 4));

    // Parse subformat
    subformat_ = bytes_to_u16(buffer_span.subspan(44, 2));
    if (!(subformat_ == kWaveFormatPCM ||
          subformat_ == kWaveFormatIEEEFloat)) {
      ESP_LOGW(kTag, "WAVE extensible subformat_ not supported");
      return cpp::fail(Error::kUnsupportedFormat);
    }
  }

  // 64 bit float is not implemented yet, make sure we're not letting it through
  if (GetFormat() == kWaveFormatIEEEFloat && bytes_per_sample_ == 8) {
    ESP_LOGW(kTag, "WAVE 64-Bit Float not supported");
    return cpp::fail(Error::kUnsupportedFormat);
  }

  int64_t data_offset = offset * samples_per_second * bytes_per_sample_;

  // Seek track to start of data
  input->SeekTo(data_chunk_index + 8 + data_offset, IStream::SeekFrom::kStartOfStream);

  output_format_ = {.num_channels = (uint8_t)num_channels_,
                    .sample_rate_hz = samples_per_second,
                    .total_samples = number_of_samples};

  return output_format_;
}

auto WavDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  bool is_eof = buffer_.Refill(input_.get());
  size_t samples_written = 0;

  buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
    size_t bytes_read = buf.size_bytes();
    size_t frames_read =
        bytes_read / bytes_per_sample_ / output_format_.num_channels;

    samples_written =
        std::min<size_t>(frames_read,
                         output.size() / output_format_.num_channels) *
        output_format_.num_channels;

    // For each sample that we're going to write
    for (size_t i = 0; i < samples_written; i++) {
      auto data = buf.subspan(i * bytes_per_sample_, bytes_per_sample_);
      if (GetFormat() == kWaveFormatPCM) {
        // PCM
        output[i] = convert_to_16_bit(data);
      } else if (GetFormat() == kWaveFormatIEEEFloat) {
        // 32-Bit Float
        if (bytes_per_sample_ == 4) {
          output[i] = convert_f32_to_16_bit(data);
        }
      }
    }

    return samples_written * bytes_per_sample_;
  });


  return OutputInfo{.samples_written = samples_written,
                    .is_stream_finished = samples_written == 0 && is_eof};
}

auto codecs::WavDecoder::GetFormat() const -> uint16_t {
  if (wave_format_ == kWaveFormatExtensible) {
    return subformat_;
  }
  return wave_format_;
}

}  // namespace codecs
