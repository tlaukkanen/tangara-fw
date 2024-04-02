/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class SourceBuffer {
 public:
  SourceBuffer();
  ~SourceBuffer();

  auto Refill(IStream* src) -> bool;
  auto AddBytes(std::function<size_t(cpp::span<std::byte>)> writer) -> void;
  auto ConsumeBytes(std::function<size_t(cpp::span<std::byte>)> reader) -> void;
  auto Empty() -> void;

  SourceBuffer(const SourceBuffer&) = delete;
  SourceBuffer& operator=(const SourceBuffer&) = delete;

 private:
  const cpp::span<std::byte> buffer_;
  size_t bytes_in_buffer_;
  size_t offset_of_bytes_;
};

}  // namespace codecs
