#pragma once

#include <string>

namespace database {

struct FileInfo {
  bool is_playable;
  std::string title;
};

auto GetInfo(const std::string &path, FileInfo *out) -> bool;

}  // namespace database
