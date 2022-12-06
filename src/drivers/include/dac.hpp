#pragma once

#include <stdint.h>

#include <functional>

#include "esp_err.h"
#include "freertos/portmacro.h"
#include "hal/i2s_types.h"
#include "result.hpp"
#include "span.hpp"

#include "gpio_expander.hpp"

namespace drivers {

/**
 * Interface for a PCM5122PWR DAC, configured over I2C.
 */
class AudioDac {
 public:
  enum Error {
    FAILED_TO_BOOT,
    FAILED_TO_CONFIGURE,
    FAILED_TO_INSTALL_I2S,
  };

  static auto create(GpioExpander* expander)
      -> cpp::result<std::unique_ptr<AudioDac>, Error>;

  AudioDac(GpioExpander* gpio);
  ~AudioDac();

  /**
   * Sets the volume on a scale from 0 (loudest) to 254 (quietest). A value of
   * 255 engages the soft mute function.
   */
  void WriteVolume(uint8_t volume);

  enum PowerState {
    POWERDOWN = 0b0,
    WAIT_FOR_CP = 0b1,
    CALIBRATION_1 = 0b10,
    CALIBRATION_2 = 0b11,
    RAMP_UP = 0b100,
    RUN = 0b101,
    SHORT = 0b110,
    RAMP_DOWN = 0b111,
    STANDBY = 0b1000,
  };

  /* Returns the current boot-up status and internal state of the DAC */
  std::pair<bool, PowerState> ReadPowerState();

  enum BitsPerSample {
    BPS_16 = I2S_BITS_PER_SAMPLE_16BIT,
    BPS_24 = I2S_BITS_PER_SAMPLE_24BIT,
    BPS_32 = I2S_BITS_PER_SAMPLE_32BIT
  };
  enum SampleRate {
    SAMPLE_RATE_44_1 = 44100,
    SAMPLE_RATE_48 = 48000,
  };

  // TODO(jacqueline): worth supporting channels here as well?
  auto Reconfigure(BitsPerSample bps, SampleRate rate) -> bool;

  auto WriteData(const cpp::span<std::byte>& data, TickType_t max_wait)
      -> std::size_t;

  // Not copyable or movable.
  AudioDac(const AudioDac&) = delete;
  AudioDac& operator=(const AudioDac&) = delete;

 private:
  GpioExpander* gpio_;

  /*
   * Pools the power state for up to 10ms, waiting for the given predicate to
   * be true.
   */
  bool WaitForPowerState(std::function<bool(bool, PowerState)> predicate);

  enum Register {
    PAGE_SELECT = 0,
    DE_EMPHASIS = 7,
    DIGITAL_VOLUME_L = 61,
    DIGITAL_VOLUME_R = 62,
    DSP_BOOT_POWER_STATE = 118,
  };

  void WriteRegister(Register reg, uint8_t val);
};

}  // namespace drivers
