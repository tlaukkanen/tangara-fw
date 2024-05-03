/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <utility>

#include "database/database.hpp"

namespace database {

/*
 * Utility to simplify waiting for a std::future to complete without blocking.
 * Each instance is good for a single future, and does not directly own anything
 * other than the future itself.
 */
template <typename T>
class FutureFetcher {
 public:
  explicit FutureFetcher(std::future<T>&& fut)
      : is_consumed_(false), fut_(std::move(fut)) {}

  /*
   * Returns whether or not the underlying future is still awaiting async work.
   */
  auto Finished() -> bool {
    if (!fut_.valid()) {
      return true;
    }
    if (fut_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      return false;
    }
    return true;
  }

  /*
   * Returns the result of the future, and releases ownership of the underling
   * resource. Will return an absent value if the future became invalid (e.g.
   * the promise associated with it was destroyed.)
   */
  auto Result() -> std::optional<T> {
    assert(!is_consumed_);
    if (is_consumed_) {
      return {};
    }
    is_consumed_ = true;
    if (!fut_.valid()) {
      return {};
    }
    return fut_.get();
  }

 private:
  bool is_consumed_;
  std::future<T> fut_;
};

}  // namespace database
