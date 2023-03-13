#include "touchwheel.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"

#include "i2c.hpp"

namespace drivers {

static const char* kTag = "TOUCHWHEEL";
static const uint8_t kTouchWheelAddress = 0x1C;

static const uint8_t kWriteMask = 0x80;
static const uint8_t kReadMask = 0xA0;

double normalise(uint16_t min, uint16_t max, uint16_t value) {
  if (value >= max) {
    return 1.0;
  }
  if (value <= min) {
    return 0.0;
  }
  uint16_t range = max - min;
  return (double)(value - min) / range;
}


auto TouchWheel::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<TouchWheel>, Error> {
  std::unique_ptr<TouchWheel> wheel = std::make_unique<TouchWheel>(expander);
  wheel->WriteRegister(Register::SLIDER_OPTIONS, 0xC0);
  return wheel;
}

TouchWheel::TouchWheel(GpioExpander* gpio) {
  this->gpio_ = gpio;
};

TouchWheel::~TouchWheel(){
};

void TouchWheel::WriteRegister(uint8_t reg, uint8_t val) {
  // uint8_t maskedReg = reg | kWriteMask;
  uint8_t maskedReg = reg;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg, val)
      .stop();
  ESP_ERROR_CHECK(transaction.Execute());
}

void TouchWheel::ReadRegister(uint8_t reg, uint8_t* data, uint8_t count) {
  // uint8_t maskedReg = reg | kReadMask;
  uint8_t maskedReg = reg;

  if (count <= 0) {
    return;
  }

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg)
      .stop()
      .start()
      .write_addr(kTouchWheelAddress, I2C_MASTER_READ)
      .read(data, I2C_MASTER_NACK)
      .stop();

  // TODO: Handle errors here.
  ESP_ERROR_CHECK(transaction.Execute());
}

void TouchWheel::Update() {
    // Read data from device into member struct
    uint8_t position;
    this->ReadRegister(Register::SLIDER_POSITION, &position, 1);
    data_.wheel_position = position;
}

TouchWheelData TouchWheel::GetTouchWheelData() const {
  return data_;
}


}  // namespace drivers
