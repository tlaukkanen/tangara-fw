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

#include "tasks.hpp"

namespace database {

/*
 * Iterator that recursively stats every file within the given directory root.
 */
class CandidateIterator {
 public:
  CandidateIterator(std::string_view root);

  /*
   * Returns the next file. The stat result is placed within `out`. If the
   * iterator has finished, returns absent. This method always modifies the
   * contents of `out`, even if no file is returned.
   */
  auto next(FILINFO& out) -> std::optional<std::string>;

  // Cannot be copied or moved.
  CandidateIterator(const CandidateIterator&) = delete;
  CandidateIterator& operator=(const CandidateIterator&) = delete;

 private:
  std::mutex mut_;
  std::pmr::deque<std::pmr::string> to_explore_;
  std::optional<std::pair<std::pmr::string, FF_DIR>> current_;
};

/*
 * Utility for iterating through each file within a directory root. Iteration
 * can be sharded across several tasks.
 */
class TrackFinder {
 public:
  TrackFinder(tasks::WorkerPool&,
              size_t parallelism,
              std::function<void(FILINFO&, std::string_view)> processor,
              std::function<void()> complete_cb);

  auto launch(std::string_view root) -> void;

  // Cannot be copied or moved.
  TrackFinder(const TrackFinder&) = delete;
  TrackFinder& operator=(const TrackFinder&) = delete;

 private:
  tasks::WorkerPool& pool_;
  const size_t parallelism_;
  const std::function<void(FILINFO&, std::string_view)> processor_;
  const std::function<void()> complete_cb_;

  std::mutex workers_mutex_;
  std::unique_ptr<CandidateIterator> iterator_;
  size_t num_workers_;

  auto schedule() -> void;
};

}  // namespace database
