/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <span>

#include "mad.h"
#include "sample.hpp"
#include "source_buffer.hpp"

#include "codec.hpp"

namespace codecs {

class MadMp3Decoder : public ICodec {
 public:
  MadMp3Decoder();
  ~MadMp3Decoder();

  auto OpenStream(std::shared_ptr<IStream> input,uint32_t offset)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(std::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  MadMp3Decoder(const MadMp3Decoder&) = delete;
  MadMp3Decoder& operator=(const MadMp3Decoder&) = delete;

 private:
  auto SkipID3Tags(IStream& stream) -> void;

  struct VbrInfo {
    uint32_t length;
    std::optional<uint32_t> bytes;
    std::optional<std::span<const unsigned char, 100>> toc;
  };

  auto GetVbrInfo(const mad_header& header) -> std::optional<VbrInfo>;
  
  auto GetBytesUsed() -> std::size_t;

  std::shared_ptr<IStream> input_;
  SourceBuffer buffer_;

  std::unique_ptr<mad_stream> stream_;
  std::unique_ptr<mad_frame> frame_;
  std::unique_ptr<mad_synth> synth_;

  int current_sample_;
  bool is_eof_;
  bool is_eos_;
};

}  // namespace codecs
