#include "gpio-expander.h"

namespace gay_ipod {

GpioExpander::GpioExpander() {
}

GpioExpander::~GpioExpander() {
}

esp_err_t GpioExpander::Write() {
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return ESP_ERR_NO_MEM;
  }

  // Technically enqueuing these commands could fail, but we don't worry about
  // it because that would indicate some really very badly wrong more generally.
  i2c_master_start(handle);
  i2c_master_write_byte(handle, (PCA8575_ADDRESS << 1 | I2C_MASTER_WRITE), true);
  i2c_master_write_byte(handle, port_a_, true);
  i2c_master_write_byte(handle, port_b_, true);
  i2c_master_stop(handle);

  // TODO: timeout
  esp_err_t ret = i2c_master_cmd_begin(0, handle, 0);

  i2c_cmd_link_delete(handle);
  return ret;
}

esp_err_t GpioExpander::Read() {
  i2c_cmd_handle_t handle = i2c_cmd_link_create();
  if (handle == NULL) {
    return ESP_ERR_NO_MEM;
  }

  // Technically enqueuing these commands could fail, but we don't worry about
  // it because that would indicate some really very badly wrong more generally.
  i2c_master_start(handle);
  i2c_master_write_byte(handle, (PCA8575_ADDRESS << 1 | I2C_MASTER_READ), true);
  i2c_master_read_byte(handle, &input_a_, I2C_MASTER_ACK);
  i2c_master_read_byte(handle, &input_b_, I2C_MASTER_LAST_NACK);
  i2c_master_stop(handle);

  // TODO: timeout
  esp_err_t ret = i2c_master_cmd_begin(0, handle, 0);

  i2c_cmd_link_delete(handle);
  return ret;
}

bool GpioExpander::charge_power_ok(void) {
  // Active-low.
  return (input_a_ & (1 << 4)) == 0;
}

bool GpioExpander::headphone_detect(void) {
  // Active-low.
  return (input_b_ & (1 << 0)) == 0; // TODO.
}

uint8_t GpioExpander::key_states(void) {
  return input_b_ & 0b00111111;
}

} // namespace gay_ipod
