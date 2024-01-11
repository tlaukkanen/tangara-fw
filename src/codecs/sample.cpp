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
  // Generate `bits` random bits
  uint64_t mask = 0xFFFFFFFF;
  mask >>= 32 - bits;
  int64_t noise = static_cast<int32_t>(komirand(&sSeed1, &sSeed2) & mask);
  // Apply to the sample, then shift to 16 bit.
  return (src + noise) >> bits;
}

}  // namespace sample
