/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_playback.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

#include "driver_cache.hpp"
#include "freertos/portmacro.h"

#include "audio_decoder.hpp"
#include "audio_element.hpp"
#include "audio_task.hpp"
#include "chunk.hpp"
#include "fatfs_audio_input.hpp"
#include "gpio_expander.hpp"
#include "i2s_audio_output.hpp"
#include "pipeline.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"

namespace audio {
AudioPlayback::AudioPlayback(drivers::DriverCache* drivers)
    : file_source_(std::make_unique<FatfsAudioInput>()),
      i2s_output_(std::make_unique<I2SAudioOutput>(drivers->AcquireGpios(),
                                                   drivers->AcquireDac())) {
  AudioDecoder* codec = new AudioDecoder();
  elements_.emplace_back(codec);

  Pipeline* pipeline = new Pipeline(elements_.front().get());
  pipeline->AddInput(file_source_.get());

  task::StartPipeline(pipeline, i2s_output_.get());
  // task::StartDrain(i2s_output_.get());
}

AudioPlayback::~AudioPlayback() {}

auto AudioPlayback::Play(const std::string& filename) -> void {
  // TODO: concurrency, yo!
  file_source_->OpenFile(filename);
}

auto AudioPlayback::LogStatus() -> void {
  i2s_output_->Log();
}

}  // namespace audio
