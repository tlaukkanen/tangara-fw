#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "result.hpp"
#include "span.hpp"

#include "gpio_expander.hpp"
#include "sys/_stdint.h"

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

  AudioDac(GpioExpander* gpio, i2s_chan_handle_t i2s_handle);
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
    BPS_16 = I2S_DATA_BIT_WIDTH_16BIT,
    BPS_24 = I2S_DATA_BIT_WIDTH_24BIT,
    BPS_32 = I2S_DATA_BIT_WIDTH_32BIT,
  };
  enum SampleRate {
    SAMPLE_RATE_44_1 = 44100,
    SAMPLE_RATE_48 = 48000,
  };

  // TODO(jacqueline): worth supporting channels here as well?
  auto Reconfigure(BitsPerSample bps, SampleRate rate) -> void;

  auto WriteData(const cpp::span<const std::byte>& data) -> void;

  auto Stop() -> void;
  auto LogStatus() -> void;

  // Not copyable or movable.
  AudioDac(const AudioDac&) = delete;
  AudioDac& operator=(const AudioDac&) = delete;

 private:
  GpioExpander* gpio_;
  i2s_chan_handle_t i2s_handle_;
  bool i2s_active_;

  i2s_std_clk_config_t clock_config_;
  i2s_std_slot_config_t slot_config_;

  /*
   * Pools the power state for up to 10ms, waiting for the given predicate to
   * be true.
   */
  bool WaitForPowerState(std::function<bool(bool, PowerState)> predicate);

  enum Register {
    PAGE_SELECT = 0,
    RESET = 1,
    POWER_MODE = 2,
    PLL_ENABLE = 4,
    DE_EMPHASIS = 7,
    PLL_CLOCK_SOURCE = 13,
    DAC_CLOCK_SOURCE = 14,
    RESYNC_REQUEST = 19,
    CLOCK_ERRORS = 37,
    INTERPOLATION = 34,
    I2S_FORMAT = 40,
    DIGITAL_VOLUME_L = 61,
    DIGITAL_VOLUME_R = 62,

    SAMPLE_RATE_DETECTION = 91,
    BCK_DETECTION = 93,
    CLOCK_ERROR_STATE = 94,
    CLOCK_STATUS = 95,
    AUTO_MUTE_STATE = 108,
    SOFT_MUTE_STATE = 114,
    SAMPLE_RATE_STATE = 115,
    DSP_BOOT_POWER_STATE = 118,
  };

  void WriteRegister(Register reg, uint8_t val);
  uint8_t ReadRegister(Register reg);
};

}  // namespace drivers
