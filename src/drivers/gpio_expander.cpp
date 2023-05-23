/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "gpio_expander.hpp"

#include <cstdint>

#include "i2c.hpp"

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

}  // namespace drivers
