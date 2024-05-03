/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "random.hpp"

#include <cstdint>

#include "esp_random.h"
#include "komihash.h"

namespace util {

IRandom* sRandom = new Random();

Random::Random() {
  esp_fill_random(&seed1_, sizeof(seed1_));
  seed2_ = seed1_;

  // komirand needs four iterations to properly self-start.
  for (int i = 0; i < 4; i++) {
    Next();
  }
}

auto Random::Next() -> std::uint64_t {
  return komirand(&seed1_, &seed2_);
}

auto Random::RangeInclusive(std::uint64_t lower,
                            std::uint64_t upper) -> std::uint64_t {
  return (Next() % (upper - lower + 1)) + lower;
}

}  // namespace util
