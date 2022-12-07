#pragma once

#include <memory>

#include "audio_playback.hpp"
#include "console.hpp"
#include "storage.hpp"

namespace console {

class AppConsole : public Console {
 public:
  explicit AppConsole(audio::AudioPlayback* playback);
  virtual ~AppConsole();

  audio::AudioPlayback* playback_;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
