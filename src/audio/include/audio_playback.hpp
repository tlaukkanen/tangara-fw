#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio_element.hpp"
#include "esp_err.h"
#include "gpio_expander.hpp"
#include "result.hpp"
#include "span.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"

namespace audio {

/*
 * TODO.
 */
class AudioPlayback {
 public:
  enum Error { ERR_INIT_ELEMENT, ERR_MEM };
  static auto create(drivers::GpioExpander* expander,
                     std::shared_ptr<drivers::SdStorage> storage)
      -> cpp::result<std::unique_ptr<AudioPlayback>, Error>;

  // TODO(jacqueline): configure on the fly once we have things to configure.
  AudioPlayback();
  ~AudioPlayback();

  auto Play(const std::string& filename) -> void;

  // Not copyable or movable.
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;

 private:
  auto ConnectElements(IAudioElement* src, IAudioElement* sink) -> void;

  StreamBuffer stream_start_;
  StreamBuffer stream_end_;
  std::vector<std::unique_ptr<StreamBuffer>> element_buffers_;
};

}  // namespace audio
