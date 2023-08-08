/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "stream_info.hpp"
#include <sys/_stdint.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "esp_heap_caps.h"
#include "result.hpp"
#include "span.hpp"
#include "types.hpp"

namespace audio {

RawStream::RawStream(std::size_t size)
    : info_(),
      buffer_size_(size),
      buffer_(reinterpret_cast<std::byte*>(
          heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))) {
  assert(buffer_ != NULL);
}

RawStream::RawStream(std::size_t size, uint32_t caps)
    : info_(),
      buffer_size_(size),
      buffer_(reinterpret_cast<std::byte*>(heap_caps_malloc(size, caps))) {
  assert(buffer_ != NULL);
}

RawStream::~RawStream() {
  heap_caps_free(buffer_);
}

auto RawStream::data() -> cpp::span<std::byte> {
  return {buffer_, buffer_size_};
}

void InputStream::consume(std::size_t bytes) const {
  assert(raw_->info().bytes_in_stream() >= bytes);
  auto new_data =
      raw_->data().subspan(bytes, raw_->info().bytes_in_stream() - bytes);
  std::move(new_data.begin(), new_data.end(), raw_->data().begin());
  raw_->info().bytes_in_stream() = new_data.size_bytes();
}

const StreamInfo& InputStream::info() const {
  return raw_->info();
}

cpp::span<const std::byte> InputStream::data() const {
  return raw_->data().first(raw_->info().bytes_in_stream());
}

void OutputStream::add(std::size_t bytes) const {
  assert(raw_->info().bytes_in_stream() + bytes <= raw_->data().size_bytes());
  raw_->info().bytes_in_stream() += bytes;
}

void OutputStream::prepare(const StreamInfo::Format& new_format,
                           std::optional<uint32_t> length) {
  raw_->info().set_format(new_format);
  raw_->info().bytes_in_stream() = 0;
  raw_->info().total_length_bytes() = length;
}

const StreamInfo& OutputStream::info() const {
  return raw_->info();
}

cpp::span<std::byte> OutputStream::data() const {
  return raw_->data().subspan(raw_->info().bytes_in_stream());
}

}  // namespace audio
