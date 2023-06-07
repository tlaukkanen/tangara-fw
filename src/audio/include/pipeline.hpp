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

  auto InStreams(std::vector<RawStream>*) -> void;

  auto OutStream() -> RawStream;

  auto GetIterationOrder() -> std::vector<Pipeline*>;

  // Not copyable or movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

 private:
  IAudioElement* root_;
  std::vector<std::unique_ptr<Pipeline>> subtrees_;

  std::array<std::byte, kPipelineBufferSize> output_buffer_;
  StreamInfo output_info_;
};

}  // namespace audio
