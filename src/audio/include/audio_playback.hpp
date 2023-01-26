#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio_element.hpp"
#include "audio_element_handle.hpp"
#include "esp_err.h"
#include "gpio_expander.hpp"
#include "result.hpp"
#include "span.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"

namespace audio {

/*
 * Creates and links together audio elements into a pipeline. This is the main
 * entrypoint to playing audio on the system.
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

  /*
   * Begins playing the file at the given FatFS path. This will interrupt any
   * currently in-progress playback.
   */
  auto Play(const std::string& filename) -> void;

  // Not copyable or movable.
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;

 private:
  auto ConnectElements(IAudioElement* src, IAudioElement* sink) -> void;
  std::vector<std::unique_ptr<AudioElementHandle>> element_handles_;

  QueueHandle_t input_handle_;
};

}  // namespace audio
