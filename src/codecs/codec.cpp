#include "codec.hpp"

namespace codecs {

auto CreateCodecForExtension(std::string extension) -> cpp::result<std::unique_ptr<ICodec>, CreateCodecError> {
  return cpp::fail(UNKNOWN_EXTENSION);
}

} // namespace codecs
