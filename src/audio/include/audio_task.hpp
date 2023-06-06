/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "audio_sink.hpp"
#include "pipeline.hpp"

namespace audio {

namespace task {

auto StartPipeline(Pipeline* pipeline, IAudioSink* sink) -> void;

}  // namespace task

}  // namespace audio
