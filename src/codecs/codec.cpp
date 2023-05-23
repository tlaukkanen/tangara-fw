/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "codec.hpp"

#include <memory>
#include "mad.hpp"

namespace codecs {

auto CreateCodecForType(StreamType type)
    -> cpp::result<std::unique_ptr<ICodec>, CreateCodecError> {
  return std::make_unique<MadMp3Decoder>();  // TODO.
}

}  // namespace codecs
