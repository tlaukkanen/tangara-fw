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
  virtual auto ReadAndParseTags(const std::pmr::string& path, TrackTags* out)
      -> bool = 0;
};

class GenericTagParser : public ITagParser {
 public:
  auto ReadAndParseTags(const std::pmr::string& path, TrackTags* out)
      -> bool override;
};

class TagParserImpl : public ITagParser {
 public:
  TagParserImpl();
  auto ReadAndParseTags(const std::pmr::string& path, TrackTags* out)
      -> bool override;

 private:
  std::map<std::pmr::string, std::unique_ptr<ITagParser>> extension_to_parser_;
  GenericTagParser generic_parser_;

  /*
   * Cache of tags that have already been extracted from files. Ideally this
   * cache should be slightly larger than any page sizes in the UI.
   */
  std::mutex cache_mutex_;
  util::LruCache<16, std::pmr::string, TrackTags> cache_;

  // We could also consider keeping caches of artist name -> std::pmr::string
  // and similar. This hasn't been done yet, as this isn't a common workload in
  // any of our UI.
};

class OpusTagParser : public ITagParser {
 public:
  auto ReadAndParseTags(const std::pmr::string& path, TrackTags* out)
      -> bool override;
};

}  // namespace database
