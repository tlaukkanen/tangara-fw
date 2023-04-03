#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "audio_element.hpp"
#include "audio_sink.hpp"
#include "chunk.hpp"
#include "result.hpp"

#include "dac.hpp"
#include "gpio_expander.hpp"
#include "stream_info.hpp"

namespace audio {

class I2SAudioOutput : public IAudioSink {
 public:
  enum Error { DAC_CONFIG, I2S_CONFIG, STREAM_INIT };
  static auto create(drivers::GpioExpander* expander)
      -> cpp::result<std::unique_ptr<I2SAudioOutput>, Error>;

  I2SAudioOutput(drivers::GpioExpander* expander,
                 std::unique_ptr<drivers::AudioDac> dac);
  ~I2SAudioOutput();

  auto Configure(const StreamInfo::Format& format) -> bool override;
  auto Send(const cpp::span<std::byte>& data) -> void override;
  auto Log() -> void override;

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 private:
  auto SetVolume(uint8_t volume) -> void;

  drivers::GpioExpander* expander_;
  std::unique_ptr<drivers::AudioDac> dac_;

  std::optional<StreamInfo::Pcm> current_config_;
};

}  // namespace audio
