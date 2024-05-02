/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "database/track.hpp"
#include "lru_cache.hpp"

namespace database {

class ITagParser {
 public:
  virtual ~ITagParser() {}
  virtual auto ReadAndParseTags(std::string_view path)
      -> std::shared_ptr<TrackTags> = 0;
};

class TagParserImpl : public ITagParser {
 public:
  TagParserImpl();
  auto ReadAndParseTags(std::string_view path)
      -> std::shared_ptr<TrackTags> override;

 private:
  auto parseNew(std::string_view path) -> std::shared_ptr<TrackTags>;

  /*
   * Cache of tags that have already been extracted from files. Ideally this
   * cache should be slightly larger than any page sizes in the UI.
   */
  std::mutex cache_mutex_;
  util::LruCache<8, std::pmr::string, std::shared_ptr<TrackTags>> cache_;

  // We could also consider keeping caches of artist name -> std::string and
  // similar. This hasn't been done yet, as this isn't a common workload in
  // any of our UI.
};

}  // namespace database
