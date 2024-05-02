/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "source_buffer.hpp"
#include <sys/_stdint.h>

#include <algorithm>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "codec.hpp"

namespace codecs {

[[maybe_unused]] static constexpr char kTag[] = "dec_buf";
static constexpr size_t kBufferSize = 1024 * 16;
static constexpr size_t kReadThreshold = 1024 * 8;

SourceBuffer::SourceBuffer()
    : buffer_(reinterpret_cast<std::byte*>(
                  heap_caps_malloc(kBufferSize, MALLOC_CAP_SPIRAM)),
              kBufferSize),
      bytes_in_buffer_(0),
      offset_of_bytes_(0) {
  assert(buffer_.data() != nullptr);
}

SourceBuffer::~SourceBuffer() {
  free(buffer_.data());
}

auto SourceBuffer::Refill(IStream* src) -> bool {
  if (bytes_in_buffer_ > kReadThreshold) {
    return false;
  }
  bool eof = false;
  AddBytes([&](std::span<std::byte> buf) -> size_t {
    ssize_t bytes_read = src->Read(buf);
    // Treat read errors as EOF.
    eof = bytes_read <= 0;
    return bytes_read;
  });
  return eof;
}

auto SourceBuffer::AddBytes(std::function<size_t(std::span<std::byte>)> writer)
    -> void {
  if (offset_of_bytes_ > 0) {
    std::memmove(buffer_.data(), buffer_.data() + offset_of_bytes_,
                 bytes_in_buffer_);
    offset_of_bytes_ = 0;
  }
  size_t added_bytes = std::invoke(writer, buffer_.subspan(bytes_in_buffer_));
  assert(bytes_in_buffer_ + added_bytes <= buffer_.size_bytes());
  bytes_in_buffer_ += added_bytes;
}

auto SourceBuffer::ConsumeBytes(
    std::function<size_t(std::span<std::byte>)> reader) -> void {
  size_t bytes_consumed = std::invoke(
      reader, buffer_.subspan(offset_of_bytes_, bytes_in_buffer_));
  assert(bytes_consumed <= bytes_in_buffer_);

  bytes_in_buffer_ -= bytes_consumed;
  if (bytes_in_buffer_ == 0) {
    offset_of_bytes_ = 0;
  } else {
    offset_of_bytes_ += bytes_consumed;
  }
}

auto SourceBuffer::Empty() -> void {
  offset_of_bytes_ = 0;
  bytes_in_buffer_ = 0;
}

}  // namespace codecs
