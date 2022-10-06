#include "gpio-expander.h"
#include <cstdint>

namespace gay_ipod {

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

  // Technically enqueuing these commands could fail, but we don't worry about
  // it because that would indicate some really very badly wrong more generally.
  i2c_master_start(handle);
  i2c_master_write_byte(handle, (kPca8575Address << 1 | I2C_MASTER_WRITE), true);
  i2c_master_write_byte(handle, ports_ab.first, true);
  i2c_master_write_byte(handle, ports_ab.second, true);
  i2c_master_stop(handle);

  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, handle, kPca8575Timeout);

  i2c_cmd_link_delete(handle);
  return ret;
}

esp_err_t GpioExpander::Read() {
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return ESP_ERR_NO_MEM;
  }

  uint8_t input_a, input_b;

  // Technically enqueuing these commands could fail, but we don't worry about
  // it because that would indicate some really very badly wrong more generally.
  i2c_master_start(handle);
  i2c_master_write_byte(handle, (kPca8575Address << 1 | I2C_MASTER_READ), true);
  i2c_master_read_byte(handle, &input_a, I2C_MASTER_ACK);
  i2c_master_read_byte(handle, &input_b, I2C_MASTER_LAST_NACK);
  i2c_master_stop(handle);

  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, handle, kPca8575Timeout);

  i2c_cmd_link_delete(handle);

  inputs_ = pack(input_a, input_b);
  return ret;
}

void GpioExpander::set_pin(ChipSelect cs, bool value) {
  set_pin((Pin) cs, value);
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
  return SpiLock(*this, cs);
}

GpioExpander::SpiLock::SpiLock(GpioExpander& gpio, ChipSelect cs)
  : lock_(gpio.cs_mutex_), gpio_(gpio), cs_(cs) {
  gpio_.with([&](auto& gpio) {
      gpio.set_pin(cs_, 0);
  });
}

GpioExpander::SpiLock::~SpiLock() { 
  gpio_.with([&](auto& gpio) {
      gpio.set_pin(cs_, 1);
  });
}

} // namespace gay_ipod
