/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

namespace codecs {

enum class StreamType {
  kMp3,
  kVorbis,
  kFlac,
  kOpus,
  kWav,
};

auto StreamTypeToString(StreamType t) -> std::string;
  
}  // namespace codecs
