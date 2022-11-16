#pragma once

#include <cstdint>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_output.hpp"

namespace drivers {

class A2DPAudioOutput : IAudioOutput {
 public:
  virtual auto SetVolume(uint8_t volume) -> void;
};

}  // namespace drivers
