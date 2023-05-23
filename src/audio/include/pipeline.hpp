/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "himem.hpp"
#include "stream_info.hpp"

namespace audio {

static const std::size_t kPipelineBufferSize = 64 * 1024;

class Pipeline {
 public:
  explicit Pipeline(IAudioElement* output);
  ~Pipeline();
  auto AddInput(IAudioElement* input) -> Pipeline*;

  auto OutputElement() const -> IAudioElement*;

  auto NumInputs() const -> std::size_t;

  auto InStreams(std::vector<MappableRegion<kPipelineBufferSize>>*,
                 std::vector<RawStream>*) -> void;

  auto OutStream(MappableRegion<kPipelineBufferSize>*) -> RawStream;

  auto GetIterationOrder() -> std::vector<Pipeline*>;

 private:
  IAudioElement* root_;
  std::vector<std::unique_ptr<Pipeline>> subtrees_;

  HimemAlloc<kPipelineBufferSize> output_buffer_;
  StreamInfo output_info_;
};

}  // namespace audio
