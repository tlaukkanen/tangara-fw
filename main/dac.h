#pragma once

#include "esp_err.h"
#include "gpio-expander.h"
#include <stdint.h>
#include <functional>

namespace gay_ipod {

  static const uint8_t kPCM5122Address = 0x4C;
  static const uint8_t kPCM5122Timeout = 100 / portTICK_RATE_MS;

/**
 * Interface for a PCM5122PWR DAC, configured over I2C.
 */
class AudioDac {
  public:
    AudioDac(GpioExpander *gpio);
    ~AudioDac();

    /**
     * Performs initial configuration of the DAC and sets it to begin expecting
     * I2S audio data.
     */
    esp_err_t Start();

    /**
     * Sets the volume on a scale from 0 (loudest) to 254 (quietest). A value of
     * 255 engages the soft mute function.
     */
    void WriteVolume(uint8_t volume);

    enum PowerState {
      POWERDOWN	       = 0b0,
      WAIT_FOR_CP      = 0b1,
      CALIBRATION_1    = 0b10,
      CALIBRATION_2    = 0b11,
      RAMP_UP	       = 0b100,
      RUN	       = 0b101,
      SHORT	       = 0b110,
      RAMP_DOWN	       = 0b111,
      STANDBY	       = 0b1000,
    };

    /* Returns the current boot-up status and internal state of the DAC */
    std::pair<bool,PowerState> ReadPowerState();

    // Not copyable or movable.
    AudioDac(const AudioDac&) = delete;
    AudioDac& operator=(const AudioDac&) = delete;

  private:
    GpioExpander *gpio_;

    /*
     * Pools the power state for up to 10ms, waiting for the given predicate to
     * be true.
     */
    bool WaitForPowerState(std::function<bool(bool,PowerState)> predicate);

    enum Register {
      PAGE_SELECT	    = 0, 
      DE_EMPHASIS	    = 7,
      DIGITAL_VOLUME_L	    = 61,
      DIGITAL_VOLUME_R	    = 62,
      DSP_BOOT_POWER_STATE  = 118,
    };

    void WriteRegister(Register reg, uint8_t val);
};

} // namespace gay_ipod
