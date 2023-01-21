#include "dac.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"

#include "gpio_expander.hpp"
#include "hal/i2s_types.h"
#include "i2c.hpp"

namespace drivers {

static const char* kTag = "AUDIODAC";
static const uint8_t kPcm5122Address = 0x4C;
static const uint8_t kPcm5122Timeout = pdMS_TO_TICKS(100);
static const i2s_port_t kI2SPort = I2S_NUM_0;

static const AudioDac::SampleRate kDefaultSampleRate =
    AudioDac::SAMPLE_RATE_44_1;
static const AudioDac::BitsPerSample kDefaultBps = AudioDac::BPS_16;

auto AudioDac::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<AudioDac>, Error> {
  // TODO: tune.
  i2s_chan_handle_t i2s_handle;
  i2s_chan_config_t channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  i2s_new_channel(&channel_config, &i2s_handle, NULL);
  //
  // First, instantiate the instance so it can do all of its power on
  // configuration.
  std::unique_ptr<AudioDac> dac =
      std::make_unique<AudioDac>(expander, i2s_handle);

  // Whilst we wait for the initial boot, we can work on installing the I2S
  // driver.
  i2s_std_config_t i2s_config = {
      .clk_cfg = dac->clock_config_,
      .slot_cfg = dac->slot_config_,
      .gpio_cfg = {.mclk = GPIO_NUM_0,
                   .bclk = GPIO_NUM_26,
                   .ws = GPIO_NUM_27,
                   .dout = GPIO_NUM_5,
                   .din = I2S_GPIO_UNUSED,
                   .invert_flags =
                       {
                           .mclk_inv = false,
                           .bclk_inv = false,
                           .ws_inv = false,
                       }},
  };

  if (esp_err_t err =
          i2s_channel_init_std_mode(i2s_handle, &i2s_config) != ESP_OK) {
    ESP_LOGE(kTag, "failed to initialise i2s channel %x", err);
    return cpp::fail(Error::FAILED_TO_INSTALL_I2S);
  }

  // TODO: does starting the channel mean the dac will boot into a more
  // meaningful state?
  i2s_channel_enable(dac->i2s_handle_);

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

AudioDac::AudioDac(GpioExpander* gpio, i2s_chan_handle_t i2s_handle)
    : gpio_(gpio),
      i2s_handle_(i2s_handle),
      clock_config_(I2S_STD_CLK_DEFAULT_CONFIG(48000)),
      slot_config_(I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                   I2S_SLOT_MODE_STEREO)) {
  gpio_->set_pin(GpioExpander::AUDIO_POWER_ENABLE, true);
  gpio_->Write();
}

AudioDac::~AudioDac() {
  i2s_channel_disable(i2s_handle_);
  i2s_del_channel(i2s_handle_);
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
      ESP_LOGI(kTag, "Waiting for power state (was %d 0x%x)", result.first,
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
  i2s_channel_disable(i2s_handle_);

  slot_config_.slot_bit_width = (i2s_slot_bit_width_t)bps;
  i2s_channel_reconfig_std_slot(i2s_handle_, &slot_config_);

  // TODO: update mclk multiple as well if needed?
  clock_config_.sample_rate_hz = rate;
  i2s_channel_reconfig_std_clock(i2s_handle_, &clock_config_);

  i2s_channel_enable(i2s_handle_);
  return true;
}

auto AudioDac::WriteData(const cpp::span<std::byte>& data, TickType_t max_wait)
    -> std::size_t {
  std::size_t res = 0;
  i2s_channel_write(i2s_handle_, data.data(), data.size(), &res, max_wait);
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
