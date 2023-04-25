#pragma once

#include <stdint.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "result.hpp"
#include "span.hpp"
#include "types.hpp"

namespace codecs {

class ICodec {
 public:
  virtual ~ICodec() {}

  virtual auto CanHandleType(StreamType type) -> bool = 0;

  struct OutputFormat {
    uint8_t num_channels;
    uint8_t bits_per_sample;
    uint32_t sample_rate_hz;
  };

  virtual auto GetOutputFormat() -> OutputFormat = 0;

  enum ProcessingError { MALFORMED_DATA };

  virtual auto ResetForNewStream() -> void = 0;

  virtual auto SetInput(cpp::span<const std::byte> input) -> void = 0;

  /*
   * Returns the codec's next read position within the input buffer. If the
   * codec is out of usable data, but there is still some data left in the
   * stream, that data should be prepended to the next input buffer.
   */
  virtual auto GetInputPosition() -> std::size_t = 0;

  /*
   * Read one frame (or equivalent discrete chunk) from the input, and
   * synthesize output samples for it.
   *
   * Returns true if we are out of usable data from the input stream, or false
   * otherwise.
   */
  virtual auto ProcessNextFrame() -> cpp::result<bool, ProcessingError> = 0;

  /*
   * Writes PCM samples to the given output buffer.
   *
   * Returns the number of bytes that were written, and true if all of the
   * samples synthesized from the last call to `ProcessNextFrame` have been
   * written. If this returns false, then this method should be called again
   * after flushing the output buffer.
   */
  virtual auto WriteOutputSamples(cpp::span<std::byte> output)
      -> std::pair<std::size_t, bool> = 0;
};

enum CreateCodecError { UNKNOWN_EXTENSION };

auto CreateCodecForType(StreamType type)
    -> cpp::result<std::unique_ptr<ICodec>, CreateCodecError>;

}  // namespace codecs
