/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <functional>
#include <sstream>
#include <string>

#include "ff.h"

namespace database {

class IFileGatherer {
 public:
  virtual ~IFileGatherer() {};

  virtual auto FindFiles(
      const std::string& root,
      std::function<void(std::string_view, const FILINFO&)> cb) -> void = 0;
};

class FileGathererImpl : public IFileGatherer {
 public:
  virtual auto FindFiles(const std::string& root,
                         std::function<void(std::string_view, const FILINFO&)>
                             cb) -> void override;
};

}  // namespace database
