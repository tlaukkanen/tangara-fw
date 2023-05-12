#pragma once

#include <deque>
#include <functional>
#include <sstream>
#include <string>

#include "ff.h"

namespace database {

class IFileGatherer {
 public:
  virtual ~IFileGatherer(){};

  virtual auto FindFiles(const std::string& root,
                         std::function<void(const std::string&)> cb)
      -> void = 0;
};

class FileGathererImpl : public IFileGatherer {
 public:
  virtual auto FindFiles(const std::string& root,
                         std::function<void(const std::string&)> cb)
      -> void override;
};

}  // namespace database
