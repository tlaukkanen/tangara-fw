#include "i2c.h"
#include "assert.h"

namespace gay_ipod {

I2CTransaction::I2CTransaction() {
  handle_ = i2c_cmd_link_create();
  assert(handle_ != NULL && "failed to create command link");
}

I2CTransaction::~I2CTransaction() {
  i2c_cmd_link_delete(handle_);
}

esp_err_t I2CTransaction::Execute() {
  return i2c_master_cmd_begin(I2C_NUM_0, handle_, kI2CTimeout);
}

I2CTransaction& I2CTransaction::start() {
  ESP_ERROR_CHECK(i2c_master_start(handle_));
  return *this;
}

I2CTransaction& I2CTransaction::stop() {
  ESP_ERROR_CHECK(i2c_master_stop(handle_));
  return *this;
}

I2CTransaction& I2CTransaction::write_addr(uint8_t addr, uint8_t op) {
  write_ack(addr << 1 | op);
  return *this;
}

I2CTransaction& I2CTransaction::write_ack(uint8_t data) {
  ESP_ERROR_CHECK(i2c_master_write_byte(handle_, data, true));
  return *this;
}

I2CTransaction& I2CTransaction::read(uint8_t *dest, i2c_ack_type_t ack) {
  ESP_ERROR_CHECK(i2c_master_read_byte(handle_, dest, ack));
  return *this;
}

} // namespace gay_ipod
