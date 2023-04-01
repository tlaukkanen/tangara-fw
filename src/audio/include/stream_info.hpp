#pragma once

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

struct StreamInfo {
  // The number of bytes that are available for consumption within this
  // stream's buffer.
  std::size_t bytes_in_stream;

  // The total length of this stream, in case its source is finite (e.g. a
  // file on disk). May be absent for endless streams (internet streams,
  // generated audio, etc.)
  std::optional<std::size_t> length_bytes;

  struct Encoded {
    // The codec that this stream is associated with.
    codecs::StreamType type;

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

  typedef std::variant<Encoded, Pcm> Format;
  Format format;

  bool operator==(const StreamInfo&) const = default;
};

class RawStream {
 public:
  StreamInfo* info;
  cpp::span<std::byte> data;
  bool is_incomplete;

  RawStream(StreamInfo* i, cpp::span<std::byte> d)
      : info(i), data(d), is_incomplete(false) {}
};

/*
 * A byte buffer + associated metadata, which is not allowed to modify any of
 * the underlying data.
 */
class InputStream {
 public:
  explicit InputStream(RawStream* s) : raw_(s) {}

  void consume(std::size_t bytes) const {
    auto new_data = raw_->data.subspan(bytes);
    std::move(new_data.begin(), new_data.end(), raw_->data.begin());
    raw_->info->bytes_in_stream = new_data.size_bytes();
  }

  void mark_incomplete() const { raw_->is_incomplete = true; }

  const StreamInfo& info() const { return *raw_->info; }

  cpp::span<const std::byte> data() const {
    return raw_->data.first(raw_->info->bytes_in_stream);
  }

 private:
  RawStream* raw_;
};

class OutputStream {
 public:
  explicit OutputStream(RawStream* s) : raw_(s) {}

  void add(std::size_t bytes) const { raw_->info->bytes_in_stream += bytes; }

  bool prepare(const StreamInfo::Format& new_format) {
    if (new_format == raw_->info->format) {
      raw_->info->format = new_format;
      return true;
    }
    if (raw_->is_incomplete) {
      raw_->info->format = new_format;
      raw_->info->bytes_in_stream = 0;
      return true;
    }
    return false;
  }

  const StreamInfo& info() const { return *raw_->info; }

  cpp::span<std::byte> data() const {
    return raw_->data.subspan(raw_->info->bytes_in_stream);
  }

  bool is_incomplete() const { return raw_->is_incomplete; }

 private:
  RawStream* raw_;
};

}  // namespace audio
