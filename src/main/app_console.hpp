#pragma once

#include <memory>

#include "audio_playback.hpp"
#include "console.hpp"

namespace console {

class AppConsole : public Console {
 public:
  AppConsole(drivers::AudioPlayback* playback);
  virtual ~AppConsole();

  drivers::AudioPlayback* playback_;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
