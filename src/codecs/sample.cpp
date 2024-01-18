/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "sample.hpp"
#include <stdint.h>

#include <cstdint>

#include "komihash.h"

namespace sample {

static uint64_t sSeed1{0};
static uint64_t sSeed2{0};

auto shiftWithDither(int64_t src, uint_fast8_t bits) -> Sample {
  // FIXME: Use a better dither.
  int16_t noise = static_cast<int16_t>(komirand(&sSeed1, &sSeed2) & 1);
  return (src >> bits) ^ noise;
}

}  // namespace sample
