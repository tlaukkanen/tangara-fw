#include "codec.hpp"

#include <memory>
#include "mad.hpp"

namespace codecs {

auto CreateCodecForExtension(std::string extension)
    -> cpp::result<std::unique_ptr<ICodec>, CreateCodecError> {
  return std::make_unique<MadMp3Decoder>();  // TODO.
}

}  // namespace codecs
