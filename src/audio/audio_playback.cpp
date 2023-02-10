#include "audio_playback.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include "audio_decoder.hpp"
#include "audio_task.hpp"
#include "chunk.hpp"
#include "fatfs_audio_input.hpp"
#include "freertos/portmacro.h"
#include "gpio_expander.hpp"
#include "i2s_audio_output.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"

namespace audio {

auto AudioPlayback::create(drivers::GpioExpander* expander,
                           std::shared_ptr<drivers::SdStorage> storage)
    -> cpp::result<std::unique_ptr<AudioPlayback>, Error> {
  // Create everything
  auto source = std::make_shared<FatfsAudioInput>(storage);
  auto codec = std::make_shared<AudioDecoder>();

  auto sink_res = I2SAudioOutput::create(expander);
  if (sink_res.has_error()) {
    return cpp::fail(ERR_INIT_ELEMENT);
  }
  auto sink = sink_res.value();

  auto playback = std::make_unique<AudioPlayback>();

  // Configure the pipeline
  playback->ConnectElements(source.get(), codec.get());
  playback->ConnectElements(codec.get(), sink.get());

  // Launch!
  playback->element_handles_.push_back(StartAudioTask("src", {}, source));
  playback->element_handles_.push_back(StartAudioTask("dec", {}, codec));
  playback->element_handles_.push_back(StartAudioTask("sink", 0, sink));

  playback->input_handle_ = source->InputEventQueue();

  return playback;
}

AudioPlayback::AudioPlayback() {}

AudioPlayback::~AudioPlayback() {
  for (auto& element : element_handles_) {
    element->Quit();
  }
}

auto AudioPlayback::Play(const std::string& filename) -> void {
  StreamInfo info;
  info.path = filename;
  auto event = StreamEvent::CreateStreamInfo(input_handle_, info);
  xQueueSend(input_handle_, &event, portMAX_DELAY);
  event = StreamEvent::CreateEndOfStream(input_handle_);
  xQueueSend(input_handle_, &event, portMAX_DELAY);
}

auto AudioPlayback::ConnectElements(IAudioElement* src, IAudioElement* sink)
    -> void {
  src->OutputEventQueue(sink->InputEventQueue());
}

}  // namespace audio
