#include "dac.h"

#include "i2c.h"
#include "esp_log.h"
#include "assert.h"
#include "driver/i2c.h"
#include "gpio-expander.h"
#include "hal/i2c_types.h"
#include <cstdint>

namespace gay_ipod {

static const char* TAG = "AUDIODAC";

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
  I2CTransaction transaction;
  transaction
    .start()
    .write_addr(kPCM5122Address, I2C_MASTER_WRITE)
    // All our registers are on the first page.
    .write_ack(Register::PAGE_SELECT, 0)
    // Disable clock autoset and ignore SCK detection
    .write_ack(Register::IGNORE_ERRORS, 0x1A)
    // Set PLL clock source to BCK
    .write_ack(Register::PLL_CLOCK_SOURCE, 0x10)
    // Set DAC clock source to PLL output
    .write_ack(Register::DAC_CLOCK_SOURCE, 0x10)

    // Configure PLL
    .write_ack(Register::PLL_P, p - 1)
    .write_ack(Register::PLL_J, j)
    .write_ack(Register::PLL_D_MSB, (d >> 8) & 0x3F)
    .write_ack(Register::PLL_D_LSB, d & 0xFF)
    .write_ack(Register::PLL_R, r - 1)

    // Clock dividers
    .write_ack(Register::DSP_CLOCK_DIV, nmac - 1)
    .write_ack(Register::DAC_CLOCK_DIV, ndac - 1)
    .write_ack(Register::NCP_CLOCK_DIV, ncp - 1)
    .write_ack(Register::OSR_CLOCK_DIV, dosr - 1)

    // IDAC (nb of DSP clock cycles per sample)
    .write_ack(Register::IDAC_MSB, (idac >> 8) & 0xFF)
    .write_ack(Register::IDAC_LSB, idac & 0xFF)

    .write_ack(Register::FS_SPEED_MODE, speedMode)
    .write_ack(Register::I2S_FORMAT, i2s_format)

    .stop();

  ESP_LOGI(TAG, "Configuring DAC");
  // TODO: Handle this gracefully.
  ESP_ERROR_CHECK(transaction.Execute());

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
  uint8_t result = 0;

  I2CTransaction transaction;
  transaction
    .start()
    .write_addr(kPCM5122Address, I2C_MASTER_WRITE)
    .write_ack(DSP_BOOT_POWER_STATE)
    .start()
    .write_addr(kPCM5122Address, I2C_MASTER_READ)
    .read(&result, I2C_MASTER_NACK)
    .stop();

  ESP_ERROR_CHECK(transaction.Execute());

  return result;
}

void AudioDac::WriteVolume(uint8_t volume) {
  I2CTransaction transaction;
  transaction
    .start()
    .write_addr(kPCM5122Address, I2C_MASTER_WRITE)
    .write_ack(Register::DIGITAL_VOLUME_L, volume)
    .write_ack(Register::DIGITAL_VOLUME_R, volume)
    .stop();

  ESP_ERROR_CHECK(transaction.Execute());
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
