/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

#include "ff.h"

namespace database {

class TrackFinder {
 public:
  TrackFinder(std::string_view root);

  auto next(FILINFO&) -> std::optional<std::string>;

 private:
  std::mutex mut_;
  std::pmr::deque<std::pmr::string> to_explore_;
  std::optional<std::pair<std::pmr::string, FF_DIR>> current_;
};

}  // namespace database
