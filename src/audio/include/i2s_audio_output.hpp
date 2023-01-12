#pragma once

#include <cstdint>
#include <memory>

#include "audio_element.hpp"
#include "result.hpp"

#include "dac.hpp"
#include "gpio_expander.hpp"
#include "sys/_stdint.h"

namespace audio {

class I2SAudioOutput : public IAudioElement {
 public:
  enum Error { DAC_CONFIG, I2S_CONFIG, STREAM_INIT };
  static auto create(drivers::GpioExpander* expander)
      -> cpp::result<std::shared_ptr<I2SAudioOutput>, Error>;

  I2SAudioOutput(drivers::GpioExpander* expander,
                 std::unique_ptr<drivers::AudioDac> dac);
  ~I2SAudioOutput();

  auto InputMinChunkSize() const -> std::size_t override {
    // TODO(jacqueline): work out a good value here. Maybe similar to the total
    // DMA buffer size?
    return 128;
  }

  auto IdleTimeout() const -> TickType_t override;
  auto ProcessStreamInfo(const StreamInfo& info)
      -> cpp::result<void, AudioProcessingError> override;
  auto ProcessChunk(const cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError> override;
  auto ProcessIdle() -> cpp::result<void, AudioProcessingError> override;
  auto PrepareForPause() -> void override;

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 private:
  auto SetVolume(uint8_t volume) -> void;
  auto SetSoftMute(bool enabled) -> void;

  drivers::GpioExpander* expander_;
  std::unique_ptr<drivers::AudioDac> dac_;

  uint8_t volume_;
  bool is_soft_muted_;
};

}  // namespace audio
