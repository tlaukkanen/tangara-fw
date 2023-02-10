#pragma once

#include <memory>
#include <string>
#include <optional>

#include "audio_element.hpp"
#include "audio_element_handle.hpp"
#include "freertos/portmacro.h"

namespace audio {

struct AudioTaskArgs {
  std::shared_ptr<IAudioElement>& element;
};

auto StartAudioTask(const std::string& name,
                    std::optional<BaseType_t> core_id,
                    std::shared_ptr<IAudioElement> element)
    -> std::unique_ptr<AudioElementHandle>;

void AudioTaskMain(void* args);

}  // namespace audio
