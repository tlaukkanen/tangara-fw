#pragma once

#include <cstddef>
#include <cstdint>
namespace codecs {

  class IAudioDecoder {
    public:
      virtual ~IAudioDecoder() {}

      virtual auto ProcessData(
          uint8_t *input,
          size_t input_len,
          uint8_t *output) -> size_t = 0;
  };

} // namespace codecs
