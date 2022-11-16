#pragma once

#include <cstdint>
#include <memory>
#include "audio_common.h"
#include "audio_element.h"

namespace drivers {

class IAudioOutput {
 public:
  IAudioOutput(audio_element_handle_t element) : element_(element) {}
  virtual ~IAudioOutput() { audio_element_deinit(element_); }

  auto GetAudioElement() -> audio_element_handle_t { return element_; }

  virtual auto SetVolume(uint8_t volume) -> void = 0;
  virtual auto GetVolume() const -> uint8_t { return volume_; }

  virtual auto Configure(audio_element_info_t& info) -> void = 0;
  virtual auto SetSoftMute(bool enabled) -> void = 0;

 protected:
  audio_element_handle_t element_;
  uint8_t volume_;
};

}  // namespace drivers
