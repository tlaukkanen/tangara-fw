#include "gpio_expander.hpp"

#include "i2c.hpp"

#include <cstdint>

namespace drivers {

GpioExpander::GpioExpander() {
  ports_ = pack(kPortADefault, kPortBDefault);
  // Read and write initial values on initialisation so that we do not have a
  // strange partially-initialised state.
  // TODO: log or abort if these error; it's really bad!
  Write();
  Read();
}

GpioExpander::~GpioExpander() {}

void GpioExpander::with(std::function<void(GpioExpander&)> f) {
  f(*this);
  Write();
}

esp_err_t GpioExpander::Write() {
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return ESP_ERR_NO_MEM;
  }

  std::pair<uint8_t, uint8_t> ports_ab = unpack(ports());

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPca8575Address, I2C_MASTER_WRITE)
      .write_ack(ports_ab.first, ports_ab.second)
      .stop();

  return transaction.Execute();
}

esp_err_t GpioExpander::Read() {
  uint8_t input_a, input_b;

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPca8575Address, I2C_MASTER_READ)
      .read(&input_a, I2C_MASTER_ACK)
      .read(&input_b, I2C_MASTER_LAST_NACK)
      .stop();

  esp_err_t ret = transaction.Execute();
  inputs_ = pack(input_a, input_b);
  return ret;
}

void GpioExpander::set_pin(ChipSelect cs, bool value) {
  set_pin((Pin)cs, value);
}

void GpioExpander::set_pin(Pin pin, bool value) {
  if (value) {
    ports_ |= (1 << pin);
  } else {
    ports_ &= ~(1 << pin);
  }
}

bool GpioExpander::get_input(Pin pin) const {
  return (inputs_ & (1 << pin)) > 0;
}

GpioExpander::SpiLock GpioExpander::AcquireSpiBus(ChipSelect cs) {
  // TODO: also spi_device_acquire_bus?
  return SpiLock(*this, cs);
}

GpioExpander::SpiLock::SpiLock(GpioExpander& gpio, ChipSelect cs)
    : lock_(gpio.cs_mutex_), gpio_(gpio), cs_(cs) {
  gpio_.with([&](auto& expander) { expander.set_pin(cs_, 0); });
}

GpioExpander::SpiLock::~SpiLock() {
  gpio_.with([&](auto& expander) { expander.set_pin(cs_, 1); });
}

}  // namespace drivers
