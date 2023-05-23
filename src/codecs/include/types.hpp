/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

namespace codecs {

enum StreamType {
  STREAM_MP3,
};

auto GetStreamTypeFromFilename(std::string filename);
}  // namespace codecs
