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

// TODO: idk
static const std::size_t kMinElementBufferSize = 1024;

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
  source->InputBuffer(&playback->stream_start_);
  sink->OutputBuffer(&playback->stream_end_);
  playback->ConnectElements(source.get(), codec.get());
  playback->ConnectElements(codec.get(), sink.get());

  // Launch!
  playback->element_handles_.push_back(StartAudioTask("src", source));
  playback->element_handles_.push_back(StartAudioTask("dec", codec));
  playback->element_handles_.push_back(StartAudioTask("sink", sink));

  return playback;
}

// TODO(jacqueline): think about sizes
AudioPlayback::AudioPlayback()
    : stream_start_(128, 128), stream_end_(128, 128) {}

AudioPlayback::~AudioPlayback() {
  for (auto& element : element_handles_) {
    element->Quit();
  }
}

auto AudioPlayback::Play(const std::string& filename) -> void {
  StreamInfo info;
  info.Path(filename);

  std::array<std::byte, 128> dest;
  auto len = WriteMessage(
      TYPE_STREAM_INFO, [&](auto enc) { return info.Encode(enc); }, dest);

  if (len.has_error()) {
    // TODO.
    return;
  }

  // TODO: short delay, return error on fail
  xMessageBufferSend(*stream_start_.Handle(), dest.data(), len.value(),
                     portMAX_DELAY);
}

auto AudioPlayback::ConnectElements(IAudioElement* src, IAudioElement* sink)
    -> void {
  std::size_t chunk_size =
      std::max(src->InputMinChunkSize(), sink->InputMinChunkSize());
  std::size_t buffer_size = std::max(kMinElementBufferSize, chunk_size * 2);

  auto buffer = std::make_unique<StreamBuffer>(chunk_size, buffer_size);
  src->OutputBuffer(buffer.get());
  sink->OutputBuffer(buffer.get());
  element_buffers_.push_back(std::move(buffer));
}

}  // namespace audio
