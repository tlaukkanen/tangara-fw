/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

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

enum Command { PLAY, PAUSE, QUIT };

struct AudioTaskArgs {
  Pipeline* pipeline;
  IAudioSink* sink;
};
struct AudioDrainArgs {
  IAudioSink* sink;
  std::atomic<Command>* command;
};

extern "C" void AudioTaskMain(void* args);
extern "C" void AudioDrainMain(void* args);

auto StartPipeline(Pipeline* pipeline, IAudioSink* sink) -> void;
auto StartDrain(IAudioSink* sink) -> void;

}  // namespace task

}  // namespace audio
