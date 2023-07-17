/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <bitset>
#include <cstdint>
#include <functional>

namespace util {

template <typename T>
class BloomFilter {
 public:
  explicit BloomFilter(std::function<uint64_t(T)> hasher)
      : hasher_(hasher), bits_() {}

  auto Insert(T val) -> void {
    uint64_t hash = std::invoke(hasher_, val);
    bits_[hash & 0xFFFF] = 1;
    bits_[(hash >> 16) & 0xFFFF] = 1;
    bits_[(hash >> 32) & 0xFFFF] = 1;
    bits_[(hash >> 48) & 0xFFFF] = 1;
  }

  auto Contains(T val) -> bool {
    uint64_t hash = std::invoke(hasher_, val);
    return bits_[hash & 0xFFFF] && bits_[(hash >> 16) & 0xFFFF] &&
           bits_[(hash >> 32) & 0xFFFF] && bits_[(hash >> 48) & 0xFFFF];
  }

 private:
  std::function<uint64_t(T)> hasher_;
  std::bitset<(1 << 16)> bits_;
};

}  // namespace util
