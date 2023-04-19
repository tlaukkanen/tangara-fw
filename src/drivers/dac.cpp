#include "dac.hpp"

#include <cstdint>
#include <cstring>

#include "assert.h"
#include "driver/i2c.h"
#include "driver/i2s_types_legacy.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "gpio_expander.hpp"
#include "hal/i2s_types.h"
#include "i2c.hpp"
#include "soc/clk_tree_defs.h"
#include "sys/_stdint.h"

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
      I2S_CHANNEL_DEFAULT_CONFIG(kI2SPort, I2S_ROLE_MASTER);
  channel_config.auto_clear = true;

  ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &i2s_handle, NULL));
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
                   //.mclk = I2S_GPIO_UNUSED,
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

  // gpio_set_direction(GPIO_NUM_0, GPIO_MODE_OUTPUT);
  // gpio_set_level(GPIO_NUM_0, 0);

  if (esp_err_t err =
          i2s_channel_init_std_mode(i2s_handle, &i2s_config) != ESP_OK) {
    ESP_LOGE(kTag, "failed to initialise i2s channel %x", err);
    return cpp::fail(Error::FAILED_TO_INSTALL_I2S);
  }

  // Make sure the DAC has booted before sending commands to it.
  bool is_booted = dac->WaitForPowerState(
      [](bool booted, PowerState state) { return booted; });
  if (!is_booted) {
    ESP_LOGE(kTag, "Timed out waiting for boot");
    return cpp::fail(Error::FAILED_TO_BOOT);
  }

  // The DAC should be booted but in power down mode, but it might not be if we
  // didn't shut down cleanly. Reset it to ensure it is in a consistent state.
  dac->WriteRegister(pcm512x::POWER, 1 << 4);
  dac->WriteRegister(pcm512x::RESET, 0b10001);

  // Use BCK for the internal PLL.
  // dac->WriteRegister(Register::PLL_CLOCK_SOURCE, 1 << 4);
  // dac->WriteRegister(Register::DAC_CLOCK_SOURCE, 0b11 << 5);

  // dac->WriteRegister(Register::PLL_ENABLE, 0);
  // dac->WriteRegister(Register::DAC_CLOCK_SOURCE, 0b0110000);
  // dac->WriteRegister(Register::CLOCK_ERRORS, 0b01000001);
  // dac->WriteRegister(Register::I2S_FORMAT, 0b110000);
  //  dac->WriteRegister(Register::INTERPOLATION, 1 << 4);

  dac->Reconfigure(BPS_16, SAMPLE_RATE_44_1);

  // Now configure the DAC for standard auto-clock SCK mode.

  // Enable auto clocking, and do your best to carry on despite errors.
  // dac->WriteRegister(Register::CLOCK_ERRORS, 0b1111101);

  // i2s_channel_enable(dac->i2s_handle_);

  dac->WaitForPowerState([](bool booted, PowerState state) {
    return state == RUN || state == STANDBY;
  });

  return dac;
}

AudioDac::AudioDac(GpioExpander* gpio, i2s_chan_handle_t i2s_handle)
    : gpio_(gpio),
      i2s_handle_(i2s_handle),
      i2s_active_(false),
      active_page_(),
      clock_config_(I2S_STD_CLK_DEFAULT_CONFIG(44100)),
      slot_config_(I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO)) {
  clock_config_.clk_src = I2S_CLK_SRC_PLL_160M;
  gpio_->set_pin(GpioExpander::AMP_EN, true);
  gpio_->Write();
}

AudioDac::~AudioDac() {
  i2s_channel_disable(i2s_handle_);
  i2s_del_channel(i2s_handle_);
  gpio_->set_pin(GpioExpander::AMP_EN, false);
  gpio_->Write();
}

void AudioDac::WriteVolume(uint8_t volume) {
  // Left channel.
  WriteRegister(pcm512x::DIGITAL_VOLUME_2, volume);
  // Right channel.
  WriteRegister(pcm512x::DIGITAL_VOLUME_3, volume);
}

std::pair<bool, AudioDac::PowerState> AudioDac::ReadPowerState() {
  uint8_t result = ReadRegister(pcm512x::POWER_STATE);
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
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }
  return has_matched;
}

auto AudioDac::Reconfigure(BitsPerSample bps, SampleRate rate) -> void {
  if (i2s_active_) {
    WriteRegister(pcm512x::POWER, 1 << 4);
    i2s_channel_disable(i2s_handle_);
  }

  // I2S reconfiguration.

  slot_config_.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
  ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(i2s_handle_, &slot_config_));

  clock_config_.sample_rate_hz = 44100;
  // If we have an MCLK/SCK, then it must be a multiple of both the sample rate
  // and the bit clock. At 24 BPS, we therefore have to change the MCLK multiple
  // to avoid issues at some sample rates. (e.g. 48KHz)
  clock_config_.mclk_multiple =
      bps == BPS_24 ? I2S_MCLK_MULTIPLE_384 : I2S_MCLK_MULTIPLE_256;
  ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(i2s_handle_, &clock_config_));

  // TODO: base on BPS
  // WriteRegister(Register::I2S_FORMAT, 0b110000);

  // Configuration is all done, so we can now bring the DAC and I2S stream back
  // up. I2S first, since otherwise the DAC will see that there's no clocks and
  // shut itself down.
  ESP_ERROR_CHECK(i2s_channel_enable(i2s_handle_));
  WriteRegister(pcm512x::POWER, 0);
  WriteRegister(pcm512x::SYNCHRONIZE, 1);
  vTaskDelay(pdMS_TO_TICKS(10));
  WriteRegister(pcm512x::SYNCHRONIZE, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  LogStatus();
  i2s_active_ = true;
}

auto AudioDac::WriteData(const cpp::span<const std::byte>& data) -> void {
  std::size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(i2s_handle_, data.data(), data.size_bytes(),
                                    &bytes_written, portMAX_DELAY);
  if (err != ESP_ERR_TIMEOUT) {
    ESP_ERROR_CHECK(err);
  }
}

auto AudioDac::Stop() -> void {
  LogStatus();
  WriteRegister(pcm512x::POWER, 1 << 4);
  i2s_channel_disable(i2s_handle_);
}

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                \
  (byte & 0x80 ? '1' : '0'), (byte & 0x40 ? '1' : '0'),     \
      (byte & 0x20 ? '1' : '0'), (byte & 0x10 ? '1' : '0'), \
      (byte & 0x08 ? '1' : '0'), (byte & 0x04 ? '1' : '0'), \
      (byte & 0x02 ? '1' : '0'), (byte & 0x01 ? '1' : '0')

auto AudioDac::LogStatus() -> void {
  uint8_t res;

  res = ReadRegister(pcm512x::RATE_DET_1);
  ESP_LOGI(kTag, "detected sample rate (want 3): %u", (res & 0b01110000) >> 4);
  ESP_LOGI(kTag, "detected SCK ratio (want 6): %u", res && 0b1111);

  res = ReadRegister(pcm512x::RATE_DET_3);
  ESP_LOGI(kTag, "detected BCK (want... 16? 32?): %u", res);

  res = ReadRegister(pcm512x::RATE_DET_4);
  ESP_LOGI(kTag, "clock errors (want zeroes): ");
  ESP_LOGI(kTag, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(res & 0b1111111));

  res = ReadRegister(pcm512x::CLOCK_STATUS);
  ESP_LOGI(kTag, "clock status (want zeroes): ");
  ESP_LOGI(kTag, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(res & 0b10111));

  res = ReadRegister(pcm512x::DIGITAL_MUTE_DET);
  ESP_LOGI(kTag, "automute status (want 0): %u", res & 0b10001);

  auto power = ReadPowerState();
  ESP_LOGI(kTag, "current power state (want 5): %u", power.second);
}

void AudioDac::WriteRegister(pcm512x::Register r, uint8_t val) {
  SelectPage(r.page);
  WriteRegisterRaw(r.reg, val);
}

uint8_t AudioDac::ReadRegister(pcm512x::Register r) {
  SelectPage(r.page);
  return ReadRegisterRaw(r.reg);
}

void AudioDac::SelectPage(uint8_t page) {
  if (active_page_ && active_page_ == page) {
    return;
  }
  WriteRegisterRaw(0, page);
  active_page_ = page;
}

void AudioDac::WriteRegisterRaw(uint8_t reg, uint8_t val) {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPcm5122Address, I2C_MASTER_WRITE)
      .write_ack(reg, val)
      .stop();
  // TODO: Retry once?
  transaction.Execute();
}

uint8_t AudioDac::ReadRegisterRaw(uint8_t reg) {
  uint8_t result = 0;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPcm5122Address, I2C_MASTER_WRITE)
      .write_ack(reg)
      .start()
      .write_addr(kPcm5122Address, I2C_MASTER_READ)
      .read(&result, I2C_MASTER_NACK)
      .stop();

  transaction.Execute();
  return result;
}

}  // namespace drivers
