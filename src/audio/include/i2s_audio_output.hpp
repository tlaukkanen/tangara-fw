#pragma once

#include <cstdint>
#include <memory>

#include "audio_element.hpp"
#include "chunk.hpp"
#include "result.hpp"

#include "dac.hpp"
#include "gpio_expander.hpp"

namespace audio {

class I2SAudioOutput : public IAudioElement {
 public:
  enum Error { DAC_CONFIG, I2S_CONFIG, STREAM_INIT };
  static auto create(drivers::GpioExpander* expander)
      -> cpp::result<std::shared_ptr<I2SAudioOutput>, Error>;

  I2SAudioOutput(drivers::GpioExpander* expander,
                 std::unique_ptr<drivers::AudioDac> dac);
  ~I2SAudioOutput();

  auto HasUnprocessedInput() -> bool override;
  auto IsOverBuffered() -> bool override;

  auto ProcessStreamInfo(const StreamInfo& info) -> void override;
  auto ProcessChunk(const cpp::span<std::byte>& chunk) -> void override;
  auto ProcessEndOfStream() -> void override;
  auto ProcessLogStatus() -> void override;
  auto Process() -> void override;

  I2SAudioOutput(const I2SAudioOutput&) = delete;
  I2SAudioOutput& operator=(const I2SAudioOutput&) = delete;

 private:
  auto SetVolume(uint8_t volume) -> void;

  drivers::GpioExpander* expander_;
  std::unique_ptr<drivers::AudioDac> dac_;

  std::optional<ChunkReader> chunk_reader_;
  cpp::span<std::byte> latest_chunk_;
};

}  // namespace audio
