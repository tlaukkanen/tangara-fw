/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "komihash.h"

namespace util {

class IRandom {
 public:
  virtual ~IRandom() {}

  virtual auto Next() -> std::uint64_t = 0;
  virtual auto RangeInclusive(std::uint64_t lower, std::uint64_t upper)
      -> std::uint64_t = 0;
};

extern IRandom* sRandom;

class Random : public IRandom {
 public:
  Random();

  auto Next() -> std::uint64_t override;
  auto RangeInclusive(std::uint64_t lower, std::uint64_t upper)
      -> std::uint64_t override;

 private:
  std::uint64_t seed1_;
  std::uint64_t seed2_;
};

}  // namespace util
