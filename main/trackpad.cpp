#include "trackpad.hpp"

#include "gpio-expander.hpp"
#include "i2c.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"

namespace gay_ipod {

static const char* kTag = "AUDIODAC";
static const uint8_t kPcm5122Address = 0x4C;
static const uint8_t kPcm5122Timeout = 100 / portTICK_RATE_MS;

// Cirque's 7-bit I2C peripheral address
static const uint8_t trackpadAddress = 0x2A;

// Masks for Cirque Register Access Protocol (RAP)
static const uint8_t writeMask = 0x80;
static const uint8_t readMask = 0xA0;

// Register config values for this demo
static const uint8_t SYSCONFIG_1 = 0x00;
static const uint8_t FEEDCONFIG_1 = 0x03;
static const uint8_t FEEDCONFIG_2 = 0x1F;
static const uint8_t Z_IDLE_COUNT = 0x05;

auto Trackpad::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<Trackpad>, Error> {
  std::unique_ptr<Trackpad> tp = std::make_unique<Trackpad>(expander);

  tp->ClearFlags();

  // Host configures bits of registers 0x03 and 0x05
  tp->WriteRegister(0x03, SYSCONFIG_1);
  tp->WriteRegister(0x05, FEEDCONFIG_2);

  // Host enables preferred output mode (absolute)
  tp->WriteRegister(0x04, FEEDCONFIG_1);

  // Host sets z-idle packet count to 5 (default is 30)
  tp->WriteRegister(0x0A, Z_IDLE_COUNT);

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
  uint8_t maskedReg = reg | writeMask;
  I2CTransaction transaction;
  transaction.start()
      .write_addr(trackpadAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg, val)
      .stop();
  // TODO: Retry once?
  ESP_ERROR_CHECK(transaction.Execute(I2C_NUM_1));
}

void Trackpad::ClearFlags() {
  this->WriteRegister(Register::STATUS1, 0x00);
}

void Trackpad::ReadRegister(uint8_t reg, uint8_t* data, uint8_t count) {
  uint8_t maskedReg = reg | readMask;

  if (count <= 0) {
    return;
  }

  I2CTransaction transaction;
  transaction.start()
      .write_addr(trackpadAddress, I2C_MASTER_WRITE)
      .write_ack(maskedReg)
      .start()
      .write_addr(trackpadAddress, I2C_MASTER_READ)
      .read(data, I2C_MASTER_NACK)
      .stop();

  ESP_ERROR_CHECK(transaction.Execute(I2C_NUM_1));
}

int Trackpad::readZLevel() {
  uint8_t data = 0;
  this->ReadRegister(Register::Z_LEVEL, &data, 1);
  this->ClearFlags();
  return data;
}

}  // namespace gay_ipod
