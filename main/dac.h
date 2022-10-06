#pragma once

#include "gpio-expander.h"
#include <stdint.h>

namespace gay_ipod {

  static const uint8_t kPCM5122Address = 0x4C;
  static const uint8_t kPCM5122Timeout = 100 / portTICK_RATE_MS;

/**
 * PCM5122PWR
 *
 * Heavily inspired from the Arduino library here:
 * https://github.com/tommag/PCM51xx_Arduino/
 */
class AudioDac {
  public:
    AudioDac(GpioExpander *gpio);
    ~AudioDac();

    /** Supported sample rates */
    enum SampleRate {
      SAMPLE_RATE_8K        = 8000,
      SAMPLE_RATE_11_025K   = 11025,
      SAMPLE_RATE_16K       = 16000,
      SAMPLE_RATE_22_05K    = 22050,
      SAMPLE_RATE_32K       = 32000,
      SAMPLE_RATE_44_1K     = 44100,
      SAMPLE_RATE_48K       = 48000,
      SAMPLE_RATE_96K       = 96000,
      SAMPLE_RATE_192K      = 192000,
      SAMPLE_RATE_384K      = 384000
    };

    enum BitDepth {
      BIT_DEPTH_16        = 16,
      BIT_DEPTH_24        = 24,
      BIT_DEPTH_32        = 32,
    };

    /**
     * Performs initial configuration of the DAC and sets it to begin expecting
     * I2S audio data.
     */
    void Start(SampleRate sample_rate, BitDepth bit_depth);

    /**
     * Sets the volume on a scale from 0 (loudest) to 254 (quietest). A value of
     * 255 engages the soft mute function.
     */
    void WriteVolume(uint8_t volume);

    enum PowerMode {
      ON = 0,
      STANDBY = 0x10,
      OFF = 0x01,
    };

    void WritePowerMode(PowerMode state);

    // Not copyable or movable.
    // TODO: maybe this could be movable?
    AudioDac(const AudioDac&) = delete;
    AudioDac& operator=(const AudioDac&) = delete;

  private:
    GpioExpander *gpio_;
    PowerMode last_power_state_ = PowerMode::OFF;

    enum Register {
      PAGE_SELECT	    = 0, 
      IGNORE_ERRORS	    = 37,
      PLL_CLOCK_SOURCE	    = 13,
      DAC_CLOCK_SOURCE	    = 14,
      PLL_P		    = 20,
      PLL_J		    = 21,
      PLL_D_MSB		    = 22,
      PLL_D_LSB		    = 23,
      PLL_R		    = 24,
      DSP_CLOCK_DIV	    = 27,
      DAC_CLOCK_DIV	    = 14,
      NCP_CLOCK_DIV	    = 29,
      OSR_CLOCK_DIV	    = 30,
      IDAC_MSB		    = 35,
      IDAC_LSB		    = 36,
      FS_SPEED_MODE	    = 34,
      I2S_FORMAT	    = 40,
      DIGITAL_VOLUME_L	    = 61,
      DIGITAL_VOLUME_R	    = 62,
      DSP_BOOT_POWER_STATE  = 118,
    };

    uint8_t ReadPowerState();
};

} // namespace gay_ipod
