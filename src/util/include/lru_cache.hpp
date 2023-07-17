/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace util {

/*
 * Basic least recently used cache. Stores the `Size` most recently accessed
 * entries in memory.
 *
 * Not safe for use from multiple tasks, but all operations are constant time.
 */
template <int Size, typename K, typename V>
class LruCache {
 public:
  LruCache() : entries_(), key_to_it_() {}

  auto Put(K key, V val) -> void {
    if (key_to_it_.contains(key)) {
      // This key was already present. Overwrite by removing the previous
      // value.
      entries_.erase(key_to_it_[key]);
      key_to_it_.erase(key);
    } else if (entries_.size() >= Size) {
      // Cache is full. Evict the last entry.
      key_to_it_.erase(entries_.back().first);
      entries_.pop_back();
    }

    // Add the new value.
    entries_.push_front({key, val});
    key_to_it_[key] = entries_.begin();
  }

  auto Get(K key) -> std::optional<V> {
    if (!key_to_it_.contains(key)) {
      return {};
    }
    // Use splice() to move the entry to the front of the list. This approach
    // doesn't invalidate any of the iterators in key_to_it_, and is constant
    // time.
    auto it = key_to_it_[key];
    entries_.splice(entries_.begin(), entries_, it);
    return it->second;
  }

  auto Clear() -> void {
    entries_.clear();
    key_to_it_.clear();
  }

 private:
  std::list<std::pair<K, V>> entries_;
  std::unordered_map<K, decltype(entries_.begin())> key_to_it_;
};

}  // namespace util
