#pragma once

#include <memory>
#include <optional>
#include <string>

#include "audio_element.hpp"
#include "audio_sink.hpp"
#include "dac.hpp"
#include "freertos/portmacro.h"
#include "pipeline.hpp"
#include "stream_buffer.hpp"

namespace audio {

namespace task {

auto StartPipeline(Pipeline* pipeline, IAudioSink* sink) -> void;

}  // namespace task

}  // namespace audio
