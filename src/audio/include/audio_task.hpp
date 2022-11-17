#pragma once

#include <memory>

#include "audio_element.hpp"

namespace audio {

struct AudioTaskArgs {
  std::shared_ptr<IAudioElement>& element;
};

void audio_task(void* args);

}  // namespace audio
