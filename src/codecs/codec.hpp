#pragma once

#include <cstddef>
#include <cstdint>

#include "result.hpp"

namespace codecs {

  enum CreateCodecError {};

  auto CreateCodecForExtension(std::string extension) -> cpp::result<std::unique_ptr<ICodec>, CreateCodecError>;

  class ICodec {
    public:
      virtual ~ICodec() {}

      virtual auto CanHandleExtension(std::string extension) -> bool = 0;

      enum Error {};

      virtual auto Process(
          uint8_t *input,
          std::size_t input_len,
          uint8_t *output,
	  std::size_t output_length) -> cpp::result<size_t, Error>  = 0;
  };

} // namespace codecs
