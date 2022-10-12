#include "dac.h"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "gpio-expander.h"
#include "hal/i2c_types.h"
#include "i2c.h"

namespace gay_ipod {

static const char* kTag = "AUDIODAC";
static const uint8_t kPcm5122Address = 0x4C;
static const uint8_t kPcm5122Timeout = 100 / portTICK_RATE_MS;

auto AudioDac::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<AudioDac>, Error> {
  std::unique_ptr<AudioDac> dac = std::make_unique<AudioDac>(expander);

  bool is_booted = dac->WaitForPowerState(
      [](bool booted, PowerState state) { return booted; });
  if (!is_booted) {
    ESP_LOGE(kTag, "Timed out waiting for boot");
    return cpp::fail(Error::FAILED_TO_BOOT);
  }

  dac->WriteRegister(Register::DE_EMPHASIS, 1 << 4);
  dac->WriteVolume(100);

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

AudioDac::AudioDac(GpioExpander* gpio) {
  this->gpio_ = gpio;
};

AudioDac::~AudioDac(){
    // TODO: reset stuff like de-emphasis? Reboot the whole dac? Need to think
    // about this.
};

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

void AudioDac::WriteRegister(Register reg, uint8_t val) {
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPcm5122Address, I2C_MASTER_WRITE)
      .write_ack(reg, val)
      .stop();
  // TODO: Retry once?
  ESP_ERROR_CHECK(transaction.Execute());
}

}  // namespace gay_ipod
