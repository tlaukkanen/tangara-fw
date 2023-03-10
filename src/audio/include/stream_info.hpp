#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
    uint16_t sample_rate;

    bool operator==(const Pcm&) const = default;
  };

  std::variant<Encoded, Pcm> data;

  bool operator==(const StreamInfo&) const = default;
};

class MutableStream {
 public:
  StreamInfo* info;
  cpp::span<std::byte> data;

  MutableStream(StreamInfo* i, cpp::span<std::byte> d)
      : info(i), data(d) {}
};

/*
 * A byte buffer + associated metadata, which is not allowed to modify any of
 * the underlying data.
 */
class Stream {
 public:
  explicit Stream(const MutableStream& s) : info(*s.info), data(s.data) {}

  const StreamInfo& info;
  // `data` itself left mutable for signalling how much of the stream was
  // consumed
  cpp::span<const std::byte> data;
};

}  // namespace audio
