/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <sys/_stdint.h>
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

class StreamInfo {
 public:
  StreamInfo() : bytes_in_stream_(0), total_length_bytes_(), format_() {}

  // The number of bytes that are available for consumption within this
  // stream's buffer.
  auto bytes_in_stream() -> std::size_t& { return bytes_in_stream_; }
  auto bytes_in_stream() const -> std::size_t { return bytes_in_stream_; }

  auto total_length_bytes() -> std::optional<std::uint32_t>& {
    return total_length_bytes_;
  }
  auto total_length_bytes() const -> std::optional<std::uint32_t> {
    return total_length_bytes_;
  }

  auto total_length_seconds() -> std::optional<std::uint32_t>& {
    return total_length_seconds_;
  }
  auto total_length_seconds() const -> std::optional<std::uint32_t> {
    return total_length_seconds_;
  }

  struct Encoded {
    // The codec that this stream is associated with.
    codecs::StreamType type;

    bool operator==(const Encoded&) const = default;
  };

  /*
   * Two-channel, interleaved, 32-bit floating point pcm samples.
   */
  struct FloatingPointPcm {
    // Number of channels in this stream.
    uint8_t channels;
    // The sample rate.
    uint32_t sample_rate;

    bool operator==(const FloatingPointPcm&) const = default;
  };

  struct Pcm {
    // Number of channels in this stream.
    uint8_t channels;
    // Number of bits per sample.
    uint8_t bits_per_sample;
    // The sample rate.
    uint32_t sample_rate;

    auto real_bytes_per_sample() const -> uint8_t {
      return bits_per_sample == 16 ? 2 : 4;
    }

    bool operator==(const Pcm&) const = default;
  };

  typedef std::variant<std::monostate, Encoded, FloatingPointPcm, Pcm> Format;
  auto format() const -> const Format& { return format_; }
  auto set_format(Format f) -> void { format_ = f; }

  template <typename T>
  auto format_as() const -> std::optional<T> {
    if (std::holds_alternative<T>(format_)) {
      return std::get<T>(format_);
    }
    return {};
  }

  bool operator==(const StreamInfo&) const = default;

 private:
  std::size_t bytes_in_stream_;
  std::optional<std::uint32_t> total_length_bytes_;
  std::optional<std::uint32_t> total_length_seconds_;
  Format format_{};
};

class InputStream;
class OutputStream;

class RawStream {
 public:
  explicit RawStream(std::size_t size);
  ~RawStream();

  auto info() -> StreamInfo& { return info_; }
  auto data() -> cpp::span<std::byte>;
  template <typename T>
  auto data_as() -> cpp::span<T> {
    auto orig = data();
    return {reinterpret_cast<T*>(orig.data()), orig.size_bytes() / sizeof(T)};
  }
  auto empty() const -> bool { return info_.bytes_in_stream() == 0; }

 private:
  StreamInfo info_;
  std::size_t buffer_size_;
  std::byte* buffer_;
};

class InputStream {
 public:
  explicit InputStream(RawStream* s) : raw_(s) {}

  void consume(std::size_t bytes) const;

  const StreamInfo& info() const;

  cpp::span<const std::byte> data() const;
  template <typename T>
  auto data_as() const -> cpp::span<const T> {
    auto orig = data();
    return {reinterpret_cast<const T*>(orig.data()),
            orig.size_bytes() / sizeof(T)};
  }

 private:
  RawStream* raw_;
};

class OutputStream {
 public:
  explicit OutputStream(RawStream* s) : raw_(s) {}

  void add(std::size_t bytes) const;

  void prepare(const StreamInfo::Format& new_format,
               std::optional<uint32_t> length);

  const StreamInfo& info() const;

  cpp::span<std::byte> data() const;
  template <typename T>
  auto data_as() const -> cpp::span<T> {
    auto orig = data();
    return {reinterpret_cast<T*>(orig.data()), orig.size_bytes() / sizeof(T)};
  }

 private:
  RawStream* raw_;
};

}  // namespace audio
