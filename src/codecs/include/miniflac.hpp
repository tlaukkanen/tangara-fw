/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "miniflac.h"
#include "sample.hpp"
#include "source_buffer.hpp"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class MiniFlacDecoder : public ICodec {
 public:
  MiniFlacDecoder();
  ~MiniFlacDecoder();

  auto OpenStream(std::shared_ptr<IStream> input)
      -> cpp::result<OutputFormat, Error> override;

  auto DecodeTo(cpp::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> override;

  auto SeekTo(std::size_t target_sample) -> cpp::result<void, Error> override;

  MiniFlacDecoder(const MiniFlacDecoder&) = delete;
  MiniFlacDecoder& operator=(const MiniFlacDecoder&) = delete;

 private:
  std::shared_ptr<IStream> input_;
  SourceBuffer buffer_;

  std::unique_ptr<miniflac_t> flac_;
  std::array<int32_t*, 2> samples_by_channel_;
  std::optional<size_t> current_sample_;
};

}  // namespace codecs
