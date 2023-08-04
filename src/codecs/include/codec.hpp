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

#include "sample.hpp"
#include "result.hpp"
#include "span.hpp"
#include "types.hpp"

namespace codecs {

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

  static auto ErrorString(Error err) -> std::string {
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

    std::optional<uint32_t> duration_seconds;
    std::optional<uint32_t> bits_per_second;
  };

  /*
   * Decodes metadata or headers from the given input stream, and returns the
   * format for the samples that will be decoded from it.
   */
  virtual auto BeginStream(cpp::span<const std::byte> input)
      -> Result<OutputFormat> = 0;

  struct OutputInfo {
    std::size_t samples_written;
    bool is_finished_writing;
  };

  /*
   * Writes PCM samples to the given output buffer.
   */
  virtual auto ContinueStream(cpp::span<const std::byte> input,
                              cpp::span<sample::Sample> output)
      -> Result<OutputInfo> = 0;

  virtual auto SeekStream(cpp::span<const std::byte> input,
                          std::size_t target_sample) -> Result<void> = 0;
};

auto CreateCodecForType(StreamType type) -> std::optional<ICodec*>;

}  // namespace codecs
