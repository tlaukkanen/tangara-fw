/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "codec.hpp"

#include <memory>
#include <optional>
#include "mad.hpp"
#include "types.hpp"

namespace codecs {

auto CreateCodecForType(StreamType type) -> std::optional<ICodec*> {
  switch (type) {
    case StreamType::kMp3:
      return new MadMp3Decoder();
    default:
      return {};
  }
}

}  // namespace codecs
