/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "sample.hpp"

#include <cstdint>

#include "komihash.h"

namespace sample {

static uint64_t sSeed1{0};
static uint64_t sSeed2{0};

auto applyDither(int64_t src, uint_fast8_t bits) -> int32_t {
  uint64_t mask = 0xFFFFFFFF;                          // 32 ones
  mask >>= 32 - bits;                                  // `bits` ones
  uint64_t noise = komirand(&sSeed1, &sSeed2) & mask;  // `bits` random noise
  return std::clamp<int64_t>(src + noise, INT32_MIN, INT32_MAX);
}

}  // namespace sample
