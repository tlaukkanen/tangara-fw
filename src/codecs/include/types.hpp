#pragma once

#include <string>

namespace codecs  {

  enum StreamType {
    STREAM_MP3,
  };

  auto GetStreamTypeFromFilename(std::string filename);
}
