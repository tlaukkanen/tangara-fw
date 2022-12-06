#include "dac.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"

#include "gpio_expander.hpp"
#include "hal/i2s_types.h"
#include "i2c.hpp"

namespace drivers {

static const char* kTag = "AUDIODAC";
static const uint8_t kPcm5122Address = 0x4C;
static const uint8_t kPcm5122Timeout = 100 / portTICK_RATE_MS;
static const i2s_port_t kI2SPort = I2S_NUM_0;

static const AudioDac::SampleRate kDefaultSampleRate =
    AudioDac::SAMPLE_RATE_44_1;
static const AudioDac::BitsPerSample kDefaultBps = AudioDac::BPS_16;

auto AudioDac::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<AudioDac>, Error> {
  // First, instantiate the instance so it can do all of its power on
  // configuration.
  std::unique_ptr<AudioDac> dac = std::make_unique<AudioDac>(expander);

  // Whilst we wait for the initial boot, we can work on installing the I2S
  // driver.
  i2s_config_t i2s_config = {
      // static_cast bc esp-adf uses enums incorrectly
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LOWMED,
      // TODO(jacqueline): tune dma buffer size. this seems very smol.
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  if (esp_err_t err =
          i2s_driver_install(kI2SPort, &i2s_config, 0, NULL) != ESP_OK) {
    ESP_LOGE(kTag, "failed to configure i2s pins %x", err);
    return cpp::fail(Error::FAILED_TO_INSTALL_I2S);
  }

  i2s_pin_config_t pin_config = {.mck_io_num = GPIO_NUM_0,
                                 .bck_io_num = GPIO_NUM_26,
                                 .ws_io_num = GPIO_NUM_27,
                                 .data_out_num = GPIO_NUM_5,
                                 .data_in_num = I2S_PIN_NO_CHANGE};
  if (esp_err_t err = i2s_set_pin(kI2SPort, &pin_config) != ESP_OK) {
    ESP_LOGE(kTag, "failed to configure i2s pins %x", err);
    return cpp::fail(Error::FAILED_TO_INSTALL_I2S);
  }

  // Now let's double check that the DAC itself came up whilst we we working.
  bool is_booted = dac->WaitForPowerState(
      [](bool booted, PowerState state) { return booted; });
  if (!is_booted) {
    ESP_LOGE(kTag, "Timed out waiting for boot");
    return cpp::fail(Error::FAILED_TO_BOOT);
  }

  // Write the initial configuration.
  dac->WriteRegister(Register::DE_EMPHASIS, 1 << 4);
  dac->WriteVolume(255);

  bool is_configured =
      dac->WaitForPowerState([](bool booted, PowerState state) {
        return state == WAIT_FOR_CP || state == RAMP_UP || state == RUN ||
               state == STANDBY;
      });
  if (!is_configured) {
    return cpp::fail(Error::FAILED_TO_CONFIGURE);
  }

  return dac;
}

AudioDac::AudioDac(GpioExpander* gpio) : gpio_(gpio) {
  gpio_->set_pin(GpioExpander::AUDIO_POWER_ENABLE, true);
  gpio_->Write();
}

AudioDac::~AudioDac() {
  i2s_driver_uninstall(kI2SPort);
  gpio_->set_pin(GpioExpander::AUDIO_POWER_ENABLE, false);
  gpio_->Write();
}

void AudioDac::WriteVolume(uint8_t volume) {
  WriteRegister(Register::DIGITAL_VOLUME_L, volume);
  WriteRegister(Register::DIGITAL_VOLUME_R, volume);
}

std::pair<bool, AudioDac::PowerState> AudioDac::ReadPowerState() {
  uint8_t result = 0;

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPcm5122Address, I2C_MASTER_WRITE)
      .write_ack(DSP_BOOT_POWER_STATE)
      .start()
      .write_addr(kPcm5122Address, I2C_MASTER_READ)
      .read(&result, I2C_MASTER_NACK)
      .stop();

  ESP_ERROR_CHECK(transaction.Execute());

  bool is_booted = result >> 7;
  PowerState detail = (PowerState)(result & 0b1111);
  return std::pair(is_booted, detail);
}

bool AudioDac::WaitForPowerState(
    std::function<bool(bool, AudioDac::PowerState)> predicate) {
  bool has_matched = false;
  for (int i = 0; i < 10; i++) {
    std::pair<bool, PowerState> result = ReadPowerState();
    has_matched = predicate(result.first, result.second);
    if (has_matched) {
      break;
    } else {
      ESP_LOGI(kTag, "Waiting for power state (was %d %x)", result.first,
               (uint8_t)result.second);
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  return has_matched;
}

auto AudioDac::Reconfigure(BitsPerSample bps, SampleRate rate) -> bool {
  // TODO(jacqueline): investigate how reliable the auto-clocking of the dac
  // is. We might need to explicit reconfigure the dac here as well if it's not
  // good enough.
  i2s_set_clk(kI2SPort, rate, bps, I2S_CHANNEL_STEREO);
  return true;
}

auto AudioDac::WriteData(const cpp::span<std::byte>& data, TickType_t max_wait)
    -> std::size_t {
  std::size_t res = 0;
  i2s_write(kI2SPort, data.data(), data.size(), &res, max_wait);
  return res;
}

void AudioDac::WriteRegister(Register reg, uint8_t val) {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPcm5122Address, I2C_MASTER_WRITE)
      .write_ack(reg, val)
      .stop();
  // TODO: Retry once?
  ESP_ERROR_CHECK(transaction.Execute());
}

}  // namespace drivers
