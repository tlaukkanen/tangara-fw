#include "trackpad.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"

#include "i2c.hpp"

namespace drivers {

static const char* kTag = "TRACKPAD";
static const uint8_t kPcm5122Address = 0x4C;
static const uint8_t kPcm5122Timeout = 100 / portTICK_RATE_MS;

static const uint8_t ki2cPort = I2C_NUM_1;

// Cirque's 7-bit I2C peripheral address
static const uint8_t kTrackpadAddress = 0x2A;

// Masks for Cirque Register Access Protocol (RAP)
static const uint8_t kWriteMask = 0x80;
static const uint8_t kReadMask = 0xA0;

// Register config values for this demo
static const uint8_t kSysConfig1 = 0x00;
static const uint8_t kFeedConfig1 = 0x03;
static const uint8_t kFeedConfig2 = 0x1F;
static const uint8_t kZIdleCount = 0x05;

auto Trackpad::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<Trackpad>, Error> {
  std::unique_ptr<Trackpad> tp = std::make_unique<Trackpad>(expander);

  tp->ClearFlags();

  // Host configures bits of registers 0x03 and 0x05
  tp->WriteRegister(0x03, kSysConfig1);
  tp->WriteRegister(0x05, kFeedConfig2);

  // Host enables preferred output mode (absolute)
  tp->WriteRegister(0x04, kFeedConfig1);

  // Host sets z-idle packet count to 5 (default is 30)
  tp->WriteRegister(0x0A, kZIdleCount);

  return tp;
}

Trackpad::Trackpad(GpioExpander* gpio) {
  this->gpio_ = gpio;
};

Trackpad::~Trackpad(){
    // TODO: reset stuff like de-emphasis? Reboot the whole dac? Need to think
    // about this.
};

void Trackpad::WriteRegister(uint8_t reg, uint8_t val) {
  uint8_t maskedReg = reg | kWriteMask;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTrackpadAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg, val)
      .stop();
  // TODO: Retry once?
  ESP_ERROR_CHECK(transaction.Execute(ki2cPort));
}

void Trackpad::ClearFlags() {
  this->WriteRegister(Register::STATUS1, 0x00);
}

void Trackpad::ReadRegister(uint8_t reg, uint8_t* data, uint8_t count) {
  uint8_t maskedReg = reg | kReadMask;

  if (count <= 0) {
    return;
  }

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kTrackpadAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg)
      .start()
      .write_addr(kTrackpadAddress, I2C_MASTER_READ)
      .read(data, I2C_MASTER_NACK)
      .stop();

  ESP_ERROR_CHECK(transaction.Execute(ki2cPort));
}

void Trackpad::Update() {
  // Read trackpad data from device
  uint8_t data_ready = 0;
  uint8_t z_level = 0;
  uint8_t x_low = 0;
  uint8_t y_low = 0;
  uint8_t x_y_high = 0;
  this->ReadRegister(Register::STATUS1, &data_ready, 1);
  if ((data_ready & (1 << 2)) != 0) {
    this->ReadRegister(Register::X_LOW_BITS, &x_low, 1);
    this->ReadRegister(Register::Y_LOW_BITS, &y_low, 1);
    this->ReadRegister(Register::X_Y_HIGH_BITS, &x_y_high, 1);
    this->ReadRegister(Register::Z_LEVEL, &z_level, 1);
  }
  this->ClearFlags();
  trackpad_data_.x_position = x_low | ((x_y_high & 0x0F) << 8);
  trackpad_data_.y_position = y_low | ((x_y_high & 0xF0) << 4);
  trackpad_data_.z_level = z_level;
  trackpad_data_.is_touched = z_level != 0;
}

TrackpadData Trackpad::GetTrackpadData() const {
  return trackpad_data_;
}

}  // namespace drivers
