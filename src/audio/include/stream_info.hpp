/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/stream_buffer.h"

#include "result.hpp"
#include "span.hpp"
#include "types.hpp"

namespace audio {

struct StreamInfo {
  // The number of bytes that are available for consumption within this
  // stream's buffer.
  std::size_t bytes_in_stream{0};

  bool is_producer_finished = true;

  bool is_consumer_finished = true;

  std::optional<std::uint32_t> duration_seconds;

  std::optional<std::uint32_t> seek_to_seconds{};

  struct Encoded {
    // The codec that this stream is associated with.
    codecs::StreamType type;

    std::optional<std::size_t> duration_bytes;

    bool operator==(const Encoded&) const = default;
  };

  struct Pcm {
    // Number of channels in this stream.
    uint8_t channels;
    // Number of bits per sample.
    uint8_t bits_per_sample;
    // The sample rate.
    uint32_t sample_rate;

    bool operator==(const Pcm&) const = default;
  };

  typedef std::variant<std::monostate, Encoded, Pcm> Format;
  Format format{};

  bool operator==(const StreamInfo&) const = default;
};

class RawStream {
 public:
  StreamInfo* info;
  cpp::span<std::byte> data;

  RawStream(StreamInfo* i, cpp::span<std::byte> d) : info(i), data(d) {}
};

/*
 * A byte buffer + associated metadata, which is not allowed to modify any of
 * the underlying data.
 */
class InputStream {
 public:
  explicit InputStream(RawStream* s) : raw_(s) {}

  void consume(std::size_t bytes) const;

  bool is_producer_finished() const;

  void mark_consumer_finished() const;

  const StreamInfo& info() const;

  cpp::span<const std::byte> data() const;

 private:
  RawStream* raw_;
};

class OutputStream {
 public:
  explicit OutputStream(RawStream* s) : raw_(s) {}

  void add(std::size_t bytes) const;

  bool prepare(const StreamInfo::Format& new_format);

  void set_duration(std::size_t);

  const StreamInfo& info() const;

  cpp::span<std::byte> data() const;

  bool is_consumer_finished() const;

  void mark_producer_finished() const;

 private:
  RawStream* raw_;
};

}  // namespace audio
