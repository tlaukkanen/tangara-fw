/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "lru_cache.hpp"
#include "track.hpp"

namespace database {

class ITagParser {
 public:
  virtual ~ITagParser() {}
  virtual auto ReadAndParseTags(const std::string& path, TrackTags* out)
      -> bool = 0;
};

class TagParserImpl : public ITagParser {
 public:
  auto ReadAndParseTags(const std::string& path, TrackTags* out)
      -> bool override;

 private:
  /*
   * Cache of tags that have already been extracted from files. Ideally this
   * cache should be slightly larger than any page sizes in the UI.
   */
  util::LruCache<16, std::string, TrackTags> cache_;

  // We could also consider keeping caches of artist name -> shared_string and
  // similar. This hasn't been done yet, as this isn't a common workload in any
  // of our UI.
};

}  // namespace database
