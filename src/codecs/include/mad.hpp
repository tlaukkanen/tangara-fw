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

#include "mad.h"
#include "sample.hpp"
#include "source_buffer.hpp"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class MadMp3Decoder : public ICodec {
 public:
  MadMp3Decoder();
  ~MadMp3Decoder();

  auto OpenStream(std::shared_ptr<IStream> input)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(cpp::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  auto SeekTo(std::size_t target_sample) -> cpp::result<void, Error> override;

  MadMp3Decoder(const MadMp3Decoder&) = delete;
  MadMp3Decoder& operator=(const MadMp3Decoder&) = delete;

 private:
  auto SkipID3Tags(IStream &stream) -> void;
  auto GetVbrLength(const mad_header& header) -> std::optional<uint32_t>;
  auto GetBytesUsed() -> std::size_t;

  std::shared_ptr<IStream> input_;
  SourceBuffer buffer_;

  mad_stream stream_;
  mad_frame frame_;
  mad_synth synth_;

  int current_sample_;
  bool is_eof_;
  bool is_eos_;
};

}  // namespace codecs
