#include "i2c.hpp"
#include <cstdint>

#include "assert.h"
#include "driver/i2c.h"

namespace gay_ipod {

static constexpr int kCmdLinkSize = I2C_LINK_RECOMMENDED_SIZE(12);

I2CTransaction::I2CTransaction() {
  // Use a fixed size buffer to avoid many many tiny allocations.
  buffer_ = (uint8_t*) calloc(sizeof(uint8_t), kCmdLinkSize);
  handle_ = i2c_cmd_link_create_static(buffer_, kCmdLinkSize);
  assert(handle_ != NULL && "failed to create command link");
}

I2CTransaction::~I2CTransaction() {
  free(buffer_);
}

esp_err_t I2CTransaction::Execute(i2c_port_t port) {
  return i2c_master_cmd_begin(port, handle_, kI2CTimeout);
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

I2CTransaction& I2CTransaction::read(uint8_t* dest, i2c_ack_type_t ack) {
  ESP_ERROR_CHECK(i2c_master_read_byte(handle_, dest, ack));
  return *this;
}

}  // namespace gay_ipod
