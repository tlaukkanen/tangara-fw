/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/track_finder.hpp"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

#include "database/track_finder.hpp"
#include "ff.h"

#include "drivers/spi.hpp"
#include "memory_resource.hpp"

namespace database {

static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR must be CHAR");

CandidateIterator::CandidateIterator(std::string_view root)
    : to_explore_(&memory::kSpiRamResource) {
  to_explore_.push_back({root.data(), root.size()});
}

auto CandidateIterator::next(FILINFO& info) -> std::optional<std::string> {
  std::scoped_lock<std::mutex> lock{mut_};
  while (!to_explore_.empty() || current_) {
    if (!current_) {
      current_.emplace();

      // Get the next directory to iterate through.
      current_->first = to_explore_.front();
      to_explore_.pop_front();
      const TCHAR* next_path =
          static_cast<const TCHAR*>(current_->first.data());

      // Open it for iterating.
      FRESULT res = f_opendir(&current_->second, next_path);
      if (res != FR_OK) {
        current_.reset();
        continue;
      }
    }

    FRESULT res = f_readdir(&current_->second, &info);
    if (res != FR_OK || info.fname[0] == 0) {
      // No more files in the directory.
      f_closedir(&current_->second);
      current_.reset();
      continue;
    } else if (info.fattrib & (AM_HID | AM_SYS) || info.fname[0] == '.') {
      // System or hidden file. Ignore it and move on.
      continue;
    } else {
      // A valid file or folder.
      std::pmr::string full_path{&memory::kSpiRamResource};
      full_path += current_->first;
      full_path += "/";
      full_path += info.fname;

      if (info.fattrib & AM_DIR) {
        // This is a directory. Add it to the explore queue.
        to_explore_.push_back(full_path);
      } else {
        // This is a file! We can return now.
        return {{full_path.data(), full_path.size()}};
      }
    }
  }

  // Out of paths to explore.
  return {};
}

TrackFinder::TrackFinder(
    tasks::WorkerPool& pool,
    size_t parallelism,
    std::function<void(FILINFO&, std::string_view)> processor,
    std::function<void()> complete_cb)
    : pool_{pool},
      parallelism_(parallelism),
      processor_(processor),
      complete_cb_(complete_cb) {}

auto TrackFinder::launch(std::string_view root) -> void {
  iterator_ = std::make_unique<CandidateIterator>(root);
  num_workers_ = parallelism_;
  for (size_t i = 0; i < parallelism_; i++) {
    schedule();
  }
}

auto TrackFinder::schedule() -> void {
  pool_.Dispatch<void>([&]() {
    FILINFO info;
    auto next = iterator_->next(info);
    if (next) {
      std::invoke(processor_, info, *next);
      schedule();
    } else {
      std::scoped_lock<std::mutex> lock{workers_mutex_};
      num_workers_ -= 1;
      if (num_workers_ == 0) {
        iterator_.reset();
        std::invoke(complete_cb_);
      }
    }
  });
}

}  // namespace database
