#include "audio_playback.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

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

auto AudioPlayback::create(drivers::GpioExpander* expander)
    -> cpp::result<std::unique_ptr<AudioPlayback>, Error> {
  auto sink_res = I2SAudioOutput::create(expander);
  if (sink_res.has_error()) {
    return cpp::fail(ERR_INIT_ELEMENT);
  }
  return std::make_unique<AudioPlayback>(std::move(sink_res.value()));
}

AudioPlayback::AudioPlayback(std::unique_ptr<I2SAudioOutput> output)
    : file_source_(std::make_unique<FatfsAudioInput>()),
      i2s_output_(std::move(output)) {
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
