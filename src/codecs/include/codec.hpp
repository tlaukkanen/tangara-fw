/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <sys/_stdint.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "result.hpp"
#include "sample.hpp"
#include "span.hpp"
#include "types.hpp"

#include "memory_resource.hpp"

namespace codecs {

/*
 * Interface for an abstract source of file-like data.
 */
class IStream {
 public:
  IStream(StreamType t) : t_(t) {}
  virtual ~IStream() {}

  auto type() -> StreamType { return t_; }

  virtual auto Read(cpp::span<std::byte> dest) -> ssize_t = 0;

  virtual auto CanSeek() -> bool = 0;

  enum class SeekFrom {
    kStartOfStream,
    kEndOfStream,
    kCurrentPosition,
  };

  virtual auto SeekTo(int64_t destination, SeekFrom from) -> void = 0;

  virtual auto CurrentPosition() -> int64_t = 0;

  /*
   * Called by codecs to indicate that they've finished parsing any header data
   * within this stream, and are about to begin decoding.
   * 
   * Currently used as a hint to the readahead stream to begin prefetching file
   * data.
   */
  virtual auto SetPreambleFinished() -> void {}

 protected:
  StreamType t_;
};

/*
 * Common interface to be implemented by all audio decoders.
 */
class ICodec {
 public:
  virtual ~ICodec() {}

  /* Errors that may be returned by codecs. */
  enum class Error {
    // Indicates that more data is required before this codec can finish its
    // operation. E.g. the input buffer ends with a truncated frame.
    kOutOfInput,
    // Indicates that the data within the input buffer is fatally malformed.
    kMalformedData,

    kInternalError,
  };

  static auto ErrorString(Error err) -> std::pmr::string {
    switch (err) {
      case Error::kOutOfInput:
        return "out of input";
      case Error::kMalformedData:
        return "malformed data";
      case Error::kInternalError:
        return "internal error";
    }
    return "uhh";
  }

  /*
   * Alias for more readable return types. All codec methods, success or
   * failure, should also return the number of bytes they consumed.
   */
  template <typename T>
  using Result = std::pair<std::size_t, cpp::result<T, Error>>;

  struct OutputFormat {
    uint8_t num_channels;
    uint32_t sample_rate_hz;
    std::optional<uint32_t> total_samples;

    bool operator==(const OutputFormat&) const = default;
  };

  /*
   * Decodes metadata or headers from the given input stream, and returns the
   * format for the samples that will be decoded from it.
   */
  virtual auto OpenStream(std::shared_ptr<IStream> input)
      -> cpp::result<OutputFormat, Error> = 0;

  struct OutputInfo {
    std::size_t samples_written;
    bool is_stream_finished;
  };

  /*
   * Writes PCM samples to the given output buffer.
   */
  virtual auto DecodeTo(cpp::span<sample::Sample> destination)
      -> cpp::result<OutputInfo, Error> = 0;

  virtual auto SeekTo(size_t target_sample) -> cpp::result<void, Error> = 0;
};

auto CreateCodecForType(StreamType type) -> std::optional<ICodec*>;

}  // namespace codecs
