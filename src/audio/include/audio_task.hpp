#pragma once

#include <memory>

#include "audio_element.hpp"
#include "audio_element_handle.hpp"

namespace audio {

struct AudioTaskArgs {
  std::shared_ptr<IAudioElement>& element;
};

auto StartAudioTask(const std::string& name,
                    std::shared_ptr<IAudioElement> element)
    -> std::unique_ptr<AudioElementHandle>;

void AudioTaskMain(void* args);

}  // namespace audio
