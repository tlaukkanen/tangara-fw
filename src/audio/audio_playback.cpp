#include "audio_playback.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

#include "freertos/portmacro.h"

#include "audio_decoder.hpp"
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

auto AudioPlayback::create(drivers::GpioExpander* expander)
    -> cpp::result<std::unique_ptr<AudioPlayback>, Error> {
  // Create everything
  auto source = std::make_shared<FatfsAudioInput>();
  auto codec = std::make_shared<AudioDecoder>();

  auto sink_res = I2SAudioOutput::create(expander);
  if (sink_res.has_error()) {
    return cpp::fail(ERR_INIT_ELEMENT);
  }
  auto sink = sink_res.value();

  auto playback = std::make_unique<AudioPlayback>();

  Pipeline *pipeline = new Pipeline(sink.get());
  pipeline->AddInput(codec.get())->AddInput(source.get());

  task::Start(pipeline);

  return playback;
}

AudioPlayback::AudioPlayback() {
  // Create everything
  auto source = std::make_shared<FatfsAudioInput>();
  auto codec = std::make_shared<AudioDecoder>();

  auto sink_res = I2SAudioOutput::create(expander);
  if (sink_res.has_error()) {
    return cpp::fail(ERR_INIT_ELEMENT);
  }
  auto sink = sink_res.value();

  auto playback = std::make_unique<AudioPlayback>();

  Pipeline *pipeline = new Pipeline(sink.get());
  pipeline->AddInput(codec.get())->AddInput(source.get());

  task::Start(pipeline);

  return playback;
}

AudioPlayback::~AudioPlayback() {
  pipeline_->Quit();
}

auto AudioPlayback::Play(const std::string& filename) -> void {
  // TODO: concurrency, yo!
  file_source->OpenFile(filename);
  pipeline_->Play();
}

auto AudioPlayback::LogStatus() -> void {
  // TODO.
}

}  // namespace audio
