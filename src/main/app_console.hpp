#pragma once

#include "audio_playback.hpp"
#include "console.hpp"

#include <memory>

namespace console {

class AppConsole : public Console {
 public:
  AppConsole(std::unique_ptr<drivers::AudioPlayback> playback);
  virtual ~AppConsole();

  std::unique_ptr<drivers::AudioPlayback> playback_;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
