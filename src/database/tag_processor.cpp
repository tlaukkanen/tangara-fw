#include "tag_processor.hpp"

namespace database {

auto GetInfo(const std::string &path, FileInfo *out) -> bool {
  // TODO(jacqueline): bring in taglib for this
  if (path.ends_with(".mp3")) {
    out->is_playable = true;
    out->title = path.substr(0, path.size() - 4);
    return true;
  }
  return false;
}

} // namespace database
