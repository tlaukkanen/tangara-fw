#include "dac.h"

#include "esp_err.h"
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

esp_err_t AudioDac::Start() {
  bool is_booted = WaitForPowerState([](bool booted, PowerState state){ return booted; });
  if (!is_booted) {
    ESP_LOGE(TAG, "Timed out waiting for boot");
    return ESP_ERR_TIMEOUT;
  }

  WriteRegister(Register::DE_EMPHASIS, 1 << 4);
  WriteVolume(100);

  WaitForPowerState([](bool booted, PowerState state){
      return state == WAIT_FOR_CP || state == RAMP_UP || state == RUN || state == STANDBY;
  });

  return ESP_OK;
}

void AudioDac::WriteVolume(uint8_t volume) {
  WriteRegister(Register::DIGITAL_VOLUME_L, volume);
  WriteRegister(Register::DIGITAL_VOLUME_R, volume);
}

std::pair<bool, AudioDac::PowerState> AudioDac::ReadPowerState() {
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

  bool is_booted = result >> 7;
  PowerState detail = (PowerState) (result & 0b1111);
  return std::pair(is_booted, detail);
}

bool AudioDac::WaitForPowerState(std::function<bool(bool,AudioDac::PowerState)> predicate) {
  bool has_matched = false;
  for (int i=0; i<10; i++) {
    std::pair<bool, PowerState> result = ReadPowerState();
    has_matched = predicate(result.first, result.second);
    if (has_matched) {
      break;
    } else {
      ESP_LOGI(TAG, "Waiting for power state (was %d %x)", result.first, (uint8_t) result.second);
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  return has_matched;
}

void AudioDac::WriteRegister(Register reg, uint8_t val) {
    I2CTransaction transaction;
    transaction
      .start()
      .write_addr(kPCM5122Address, I2C_MASTER_WRITE)
      .write_ack(reg, val)
      .stop();
    // TODO: Retry once?
    ESP_ERROR_CHECK(transaction.Execute());
}

} // namespace gay_ipod
