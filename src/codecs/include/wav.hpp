/*
 * Copyright 2023 Daniel <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "sample.hpp"
#include "source_buffer.hpp"

#include "codec.hpp"

namespace codecs {

static const uint16_t kWaveFormatPCM = 0x0001;
static const uint16_t kWaveFormatIEEEFloat = 0x0003;
static const uint16_t kWaveFormatAlaw = 0x0006;
static const uint16_t kWaveFormatMulaw = 0x0007;
static const uint16_t kWaveFormatExtensible = 0xFFFE;

class WavDecoder : public ICodec {
 public:
  WavDecoder();
  ~WavDecoder();

  auto OpenStream(std::shared_ptr<IStream> input,uint32_t offset)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(std::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  WavDecoder(const WavDecoder&) = delete;
  WavDecoder& operator=(const WavDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  SourceBuffer buffer_;
  uint16_t wave_format_;
  uint16_t subformat_;
  OutputFormat output_format_;
  uint16_t bytes_per_sample_;
  uint16_t num_channels_;

  auto GetFormat() const -> uint16_t;
};

}  // namespace codecs
