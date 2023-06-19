/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "stream_info.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "result.hpp"
#include "span.hpp"
#include "types.hpp"

namespace audio {

void InputStream::consume(std::size_t bytes) const {
  assert(raw_->info->bytes_in_stream >= bytes);
  auto new_data =
      raw_->data.subspan(bytes, raw_->info->bytes_in_stream - bytes);
  std::move(new_data.begin(), new_data.end(), raw_->data.begin());
  raw_->info->bytes_in_stream = new_data.size_bytes();
}

void InputStream::mark_consumer_finished() const {
  raw_->info->is_consumer_finished = true;
}

bool InputStream::is_producer_finished() const {
  return raw_->info->is_producer_finished;
}

const StreamInfo& InputStream::info() const {
  return *raw_->info;
}

cpp::span<const std::byte> InputStream::data() const {
  return raw_->data.first(raw_->info->bytes_in_stream);
}

void OutputStream::add(std::size_t bytes) const {
  assert(raw_->info->bytes_in_stream + bytes <= raw_->data.size_bytes());
  raw_->info->bytes_in_stream += bytes;
}

bool OutputStream::prepare(const StreamInfo::Format& new_format) {
  if (std::holds_alternative<std::monostate>(raw_->info->format) ||
      raw_->info->is_consumer_finished) {
    raw_->info->format = new_format;
    raw_->info->bytes_in_stream = 0;
    raw_->info->is_producer_finished = false;
    raw_->info->is_consumer_finished = false;
    return true;
  }
  return false;
}

const StreamInfo& OutputStream::info() const {
  return *raw_->info;
}

cpp::span<std::byte> OutputStream::data() const {
  return raw_->data.subspan(raw_->info->bytes_in_stream);
}

void OutputStream::mark_producer_finished() const {
  raw_->info->is_producer_finished = true;
}

bool OutputStream::is_consumer_finished() const {
  return raw_->info->is_consumer_finished;
}

}  // namespace audio
