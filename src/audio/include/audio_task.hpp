#pragma once

#include <memory>

#include "audio_element.hpp"

namespace audio {

struct AudioTaskArgs {
  std::shared_ptr<IAudioElement>& element;
};

auto StartAudioTask(std::shared_ptr<IAudioElement>& element) -> void;

void AudioTaskMain(void* args);

}  // namespace audio
