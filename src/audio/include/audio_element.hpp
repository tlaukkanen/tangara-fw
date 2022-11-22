#pragma once

#include <stdint.h>
#include <cstdint>
#include "freertos/portmacro.h"
#include "types.hpp"
#include "result.hpp"

namespace audio {

extern const std::size_t kMaxFrameSize;

class IAudioElement {
 public:
  virtual ~IAudioElement();

  virtual auto IdleTimeout() -> TickType_t { return portMAX_DELAY; }

  virtual auto InputBuffer() -> MessageBufferHandle_t* = 0;

  virtual auto OutputBuffer() -> MessageBufferHandle_t* = 0;

  enum StreamError {
    BAD_FORMAT
  };

  virtual auto ProcessStreamInfo(StreamInfo &info) -> cpp::result<void, StreamError> = 0;
  virtual auto ProcessChunk(uint8_t* data, std::size_t length) -> cpp::result<void, StreamError> = 0;
  virtual auto ProcessIdle() -> cpp::result<void, StreamError> = 0;
};

}  // namespace audio
