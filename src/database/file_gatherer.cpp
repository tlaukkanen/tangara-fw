#include "file_gatherer.hpp"

#include <deque>
#include <functional>
#include <sstream>
#include <string>

#include "ff.h"

namespace database {

static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR must be CHAR");

auto FileGathererImpl::FindFiles(const std::string& root,
                                 std::function<void(const std::string&)> cb)
    -> void {
  std::deque<std::string> to_explore;
  to_explore.push_back(root);

  while (!to_explore.empty()) {
    std::string next_path_str = to_explore.front();
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
        std::stringstream full_path;
        full_path << next_path_str << "/" << info.fname;

        if (info.fattrib & AM_DIR) {
          // This is a directory. Add it to the explore queue.
          to_explore.push_back(full_path.str());
        } else {
          // This is a file! Let the callback know about it.
          // std::invoke(cb, full_path.str(), info);
          std::invoke(cb, full_path.str());
        }
      }
    }

    f_closedir(&dir);
    to_explore.pop_front();
  }
}

}  // namespace database
