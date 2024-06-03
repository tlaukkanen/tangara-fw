/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/file_gatherer.hpp"

#include <deque>
#include <functional>
#include <sstream>
#include <string>

#include "ff.h"

#include "drivers/spi.hpp"
#include "memory_resource.hpp"

namespace database {

static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR must be CHAR");

auto FileGathererImpl::FindFiles(
    const std::string& root,
    std::function<void(std::string_view, const FILINFO&)> cb) -> void {
  std::pmr::deque<std::pmr::string> to_explore{&memory::kSpiRamResource};
  to_explore.push_back({root.data(), root.size()});

  while (!to_explore.empty()) {
    auto next_path_str = to_explore.front();
    to_explore.pop_front();

    const TCHAR* next_path = static_cast<const TCHAR*>(next_path_str.c_str());

    FF_DIR dir;
    FRESULT res = f_opendir(&dir, next_path);
    if (res != FR_OK) {
      // TODO: log.
      continue;
    }

    for (;;) {
      FILINFO info;
      res = f_readdir(&dir, &info);
      if (res != FR_OK || info.fname[0] == 0) {
        // No more files in the directory.
        break;
      } else if (info.fattrib & (AM_HID | AM_SYS) || info.fname[0] == '.') {
        // System or hidden file. Ignore it and move on.
        continue;
      } else {
        std::pmr::string full_path{&memory::kSpiRamResource};
        full_path += next_path_str;
        full_path += "/";
        full_path += info.fname;

        if (info.fattrib & AM_DIR) {
          // This is a directory. Add it to the explore queue.
          to_explore.push_back(full_path);
        } else {
          // This is a file! Let the callback know about it.
          // std::invoke(cb, full_path.str(), info);
          std::invoke(cb, full_path, info);
        }
      }
    }

    f_closedir(&dir);
  }
}

}  // namespace database
