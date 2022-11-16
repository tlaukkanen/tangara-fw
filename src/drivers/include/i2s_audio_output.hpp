#pragma once

#include "audio_output.hpp"
#include "dac.hpp"
#include "gpio-expander.hpp"
#include "result.hpp"

#include <cstdint>
#include <memory>

#include "audio_element.h"

namespace drivers {

class I2SAudioOutput : public IAudioOutput {
 public:
  enum Error { DAC_CONFIG, I2S_CONFIG, STREAM_INIT };
  static auto create(GpioExpander* expander)
      -> cpp::result<std::unique_ptr<I2SAudioOutput>, Error>;

  I2SAudioOutput(std::unique_ptr<AudioDac>& dac,
                 audio_element_handle_t element);
  ~I2SAudioOutput();

  virtual auto SetVolume(uint8_t volume) -> void;
  virtual auto Configure(audio_element_info_t& info) -> void;
  virtual auto SetSoftMute(bool enabled) -> void;

 private:
  std::unique_ptr<AudioDac> dac_;
  bool is_soft_muted_ = false;
};

}  // namespace drivers
