#pragma once

#include <stdint.h>
#include <cstddef>
#include <cstdint>

#include "result.hpp"

namespace codecs {

enum CreateCodecError { UNKNOWN_EXTENSION };

auto CreateCodecForFile(const std::string& extension)
    -> cpp::result<std::unique_ptr<ICodec>, CreateCodecError>;

class ICodec {
 public:
  virtual ~ICodec() {}

  virtual auto CanHandleFile(const std::string& path) -> bool = 0;

  struct OutputFormat {
    uint8_t num_channels;
    uint8_t bits_per_sample;
    int sample_rate_hz;
  };

  virtual auto GetOutputFormat() -> OutputFormat = 0;

  enum ProcessingError {};

  struct Result {
    bool need_more_input;
    /*
     * For need_more_input, this is how far we got in the input buffer
     * before we were unable to process more data. Any remaining data in the
     * buffer should be moved to the start before the next call.
     */
    std::size_t input_processed;

    bool flush_output;
    /*
     * For flush_output, this is how far we got in the output buffer before
     * we ran out of space for samples. The caller should flush this many
     * bytes downstream.
     */
    std::size_t output_written;
  };

  virtual auto ResetForNewStream() -> void = 0;

  virtual auto SetInput(uint8_t* buffer, std::size_t length) = 0;
  virtual auto Process(uint8_t* output, std::size_t output_length)
      -> cpp::result<size_t, Error> = 0;

  virtual auto GetOutputProcessed() -> std::size_t;
  = 0;
};

}  // namespace codecs
