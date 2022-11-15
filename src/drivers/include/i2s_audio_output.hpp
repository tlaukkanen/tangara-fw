#pragma once

#include "audio_common.h"
#include "audio_element.h"
#include "audio_output.hpp"
#include "gpio-expander.hpp"
#include <cstdint>
#include <memory>
#include "result.hpp"
#include "dac.hpp"

namespace drivers {

class I2SAudioOutput : public IAudioOutput {
  public:
  enum Error { DAC_CONFIG, I2S_CONFIG, STREAM_INIT };
    static auto create(GpioExpander* expander)
        -> cpp::result<std::unique_ptr<I2SAudioOutput>, Error>;

  I2SAudioOutput(AudioDac* dac, audio_element_handle_t element);
  ~I2SAudioOutput();

  virtual auto SetVolume(uint8_t volume) -> void;
  virtual auto Configure(audio_element_info_t info) -> void;

  private:
    std::unique_ptr<AudioDac> dac_;
};

} // namespace drivers
