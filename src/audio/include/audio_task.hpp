#pragma once

#include <memory>
#include <optional>
#include <string>

#include "audio_element.hpp"
#include "freertos/portmacro.h"

namespace audio {

struct AudioTaskArgs {
  std::shared_ptr<IAudioElement>& element;
};

auto StartAudioTask(const std::string& name,
                    std::optional<BaseType_t> core_id,
                    std::shared_ptr<IAudioElement> element) -> void;

void AudioTaskMain(void* args);

}  // namespace audio
