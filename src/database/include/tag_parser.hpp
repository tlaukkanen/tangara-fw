/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "song.hpp"

namespace database {

class ITagParser {
 public:
  virtual ~ITagParser() {}
  virtual auto ReadAndParseTags(const std::string& path, SongTags* out)
      -> bool = 0;
};

class TagParserImpl : public ITagParser {
 public:
  virtual auto ReadAndParseTags(const std::string& path, SongTags* out)
      -> bool override;
};

}  // namespace database
