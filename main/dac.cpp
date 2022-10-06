#include "dac.h"

#include "esp_log.h"
#include "assert.h"
#include "driver/i2c.h"
#include "gpio-expander.h"
#include "hal/i2c_types.h"
#include <cstdint>

namespace gay_ipod {

static const char* TAG = "AUDIODAC";

/**
 * Utility method for writing to a register on the PCM5122. Writes two bytes:
 * first the address for the register that we're writing to, and then the value
 * to write.
 *
 * Note this function assumes that the correct page has already been selected.
 */
static void set_register(i2c_cmd_handle_t handle, uint8_t reg, uint8_t val) {
    i2c_master_write_byte(handle, reg, true);
    i2c_master_write_byte(handle, val, true);
}

AudioDac::AudioDac(GpioExpander *gpio) {
  this->gpio_ = gpio;
};

AudioDac::~AudioDac() {};

void AudioDac::Start(SampleRate sample_rate, BitDepth bit_depth) {
  // First check that the arguments look okay.

  // Check that the bit clock (PLL input) is between 1MHz and 50MHz
  uint32_t bckFreq = sample_rate * bit_depth * 2;
  if (bckFreq < 1000000 || bckFreq > 50000000) {
    return;
  }

  // 24 bits is not supported for 44.1kHz and 48kHz.
  if ((sample_rate == SAMPLE_RATE_44_1K || sample_rate == SAMPLE_RATE_48K)
      && bit_depth == BIT_DEPTH_24) {
    return;
  }

  // Next, wait for the IC to boot up before we try configuring it.
  bool is_booted = false;
  for (int i=0; i<10; i++) {
    uint8_t result = ReadPowerState();
    is_booted = result >> 7;  
    if (is_booted) {
      break;
    } else {
      ESP_LOGI(TAG, "Waiting for boot...");
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  if (!is_booted) {
    // TODO: properly handle
    ESP_LOGE(TAG, "Timed out waiting for boot!");
    return;
  }

  // Now do all the math required to correctly set up the internal clocks.
  int p, j, d, r;
  int nmac, ndac, ncp, dosr, idac;
  if (sample_rate == SAMPLE_RATE_11_025K ||
      sample_rate == SAMPLE_RATE_22_05K ||
      sample_rate == SAMPLE_RATE_44_1K) {
    //44.1kHz and derivatives.
    //P = 1, R = 2, D = 0 for all supported combinations.
    //Set J to have PLL clk = 90.3168 MHz
    p = 1;
    r = 2;
    j = 90316800 / bckFreq / r;
    d = 0;

    //Derive clocks from the 90.3168MHz PLL
    nmac = 2;
    ndac = 16;
    ncp = 4;
    dosr = 8;
    idac = 1024; // DSP clock / sample rate
  } else {
    //8kHz and multiples.
    //PLL config for a 98.304 MHz PLL clk
    if (bit_depth == BIT_DEPTH_24 && bckFreq > 1536000)
      p = 3;
    else if (bckFreq > 12288000)
      p = 2;
    else
      p = 1;

    r = 2;
    j = 98304000 / (bckFreq / p) / r;
    d = 0;

    //Derive clocks from the 98.304MHz PLL
    switch (sample_rate) {
      case SAMPLE_RATE_16K: nmac = 6; break;
      case SAMPLE_RATE_32K: nmac = 3; break;
      default:              nmac = 2; break;
    };

    ndac = 16;
    ncp = 4;
    dosr = 384000 / sample_rate;
    idac = 98304000 / nmac / sample_rate; // DSP clock / sample rate
  }

  // FS speed mode
  int speedMode;
  if (sample_rate <= SAMPLE_RATE_48K) {
    speedMode = 0;
  } else if (sample_rate <= SAMPLE_RATE_96K) {
    speedMode = 1;
  } else if (sample_rate <= SAMPLE_RATE_192K) {
    speedMode = 2;
  } else {
    speedMode = 3;
  }

  // Set correct I2S config
  uint8_t i2s_format = 0;
  switch (bit_depth) {
    case BIT_DEPTH_16: i2s_format = 0x00; break;
    case BIT_DEPTH_24: i2s_format = 0x02; break;
    case BIT_DEPTH_32: i2s_format = 0x03; break;
  };

  // We've calculated all of our data. Now assemble the big list of registers
  // that we need to configure.
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return;
  }

  i2c_master_start(handle);
  i2c_master_write_byte(handle, (kPCM5122Address << 1 | I2C_MASTER_WRITE), true);

  // All our registers are on the first page.
  set_register(handle, Register::PAGE_SELECT, 0);

  // Disable clock autoset and ignore SCK detection
  set_register(handle, Register::IGNORE_ERRORS, 0x1A);
  // Set PLL clock source to BCK
  set_register(handle, Register::PLL_CLOCK_SOURCE, 0x10);
  // Set DAC clock source to PLL output
  set_register(handle, Register::DAC_CLOCK_SOURCE, 0x10);

  // Configure PLL
  set_register(handle, Register::PLL_P, p - 1);
  set_register(handle, Register::PLL_J, j);
  set_register(handle, Register::PLL_D_MSB, (d >> 8) & 0x3F);
  set_register(handle, Register::PLL_D_LSB, d & 0xFF);
  set_register(handle, Register::PLL_R, r - 1);

  // Clock dividers
  set_register(handle, Register::DSP_CLOCK_DIV, nmac - 1);
  set_register(handle, Register::DAC_CLOCK_DIV, ndac - 1);
  set_register(handle, Register::NCP_CLOCK_DIV, ncp - 1);
  set_register(handle, Register::OSR_CLOCK_DIV, dosr - 1);

  // IDAC (nb of DSP clock cycles per sample)
  set_register(handle, Register::IDAC_MSB, (idac >> 8) & 0xFF);
  set_register(handle, Register::IDAC_LSB, idac & 0xFF);

  set_register(handle, Register::FS_SPEED_MODE, speedMode);
  set_register(handle, Register::I2S_FORMAT, i2s_format);

  i2c_master_stop(handle);

  ESP_LOGI(TAG, "Configuring DAC");
  // TODO: Handle this gracefully.
  ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, handle, kPCM5122Timeout));

  i2c_cmd_link_delete(handle);

  // The DAC takes a moment to reconfigure itself. Give it some time before we
  // start asking for its state.
  vTaskDelay(pdMS_TO_TICKS(5));

  // TODO: investigate why it's stuck waiting for CP voltage.
  /*
  bool is_configured = false;
  for (int i=0; i<10; i++) {
    uint8_t result = ReadPowerState();
    is_configured = (result & 0b1111) == 0b1001;
    if (is_configured) {
      break;
    } else {
      ESP_LOGI(TAG, "Waiting for configure...");
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  if (!is_configured) {
    // TODO: properly handle
    ESP_LOGE(TAG, "Timed out waiting for configure!");
    return;
  }
  */
}

uint8_t AudioDac::ReadPowerState() {
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return 0;
  }

  uint8_t result = 0;

  i2c_master_start(handle);
  i2c_master_write_byte(handle, (kPCM5122Address << 1 | I2C_MASTER_WRITE), true);
  i2c_master_write_byte(handle, DSP_BOOT_POWER_STATE, true);
  i2c_master_start(handle);
  i2c_master_write_byte(handle, (kPCM5122Address << 1 | I2C_MASTER_READ), true);
  i2c_master_read_byte(handle, &result, I2C_MASTER_NACK);
  i2c_master_stop(handle);

  ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, handle, kPCM5122Timeout));

  i2c_cmd_link_delete(handle);

  return result;
}

void AudioDac::WriteVolume(uint8_t volume) {
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return;
  }

  i2c_master_start(handle);
  i2c_master_write_byte(handle, (kPCM5122Address << 1 | I2C_MASTER_WRITE), true);

  set_register(handle, Register::DIGITAL_VOLUME_L, volume);
  set_register(handle, Register::DIGITAL_VOLUME_R, volume);

  i2c_master_stop(handle);

  i2c_master_cmd_begin(I2C_NUM_0, handle, 50);

  i2c_cmd_link_delete(handle);
}

void AudioDac::WritePowerMode(PowerMode mode) {
  switch (mode) {
    case ON:
    case STANDBY:
      // TODO: enable power switch.
      break;
    case OFF:
      // TODO: disable power switch.
      break;
  }
}

} // namespace gay_ipod
