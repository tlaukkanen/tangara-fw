#pragma once

#include <cstdint>

#include "driver/i2c.h"
#include "hal/i2c_types.h"

namespace gay_ipod {

/*
 * Convenience wrapper for performing an I2C transaction with a reasonable
 * preconfigured timeout, automatic management of a heap-based command buffer,
 * and a terser API for enqueuing bytes.
 *
 * Any error codes from the underlying ESP IDF are treated as fatal, since they
 * typically represent invalid arguments or OOMs.
 */
class I2CTransaction {
 public:
  static const uint8_t kI2CTimeout = 100 / portTICK_RATE_MS;

  I2CTransaction();
  ~I2CTransaction();

  /*
   * Executes all enqueued commands, returning the result code. Possible error
   * codes, per the ESP-IDF docs:
   *
   * ESP_OK Success
   * ESP_ERR_INVALID_ARG Parameter error
   * ESP_FAIL Sending command error, slave doesn’t ACK the transfer.
   * ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
   * ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
   */
  esp_err_t Execute();

  /*
   * Enqueues a start condition. May also be used for repeated start
   * conditions.
   */
  I2CTransaction& start();
  /* Enqueues a stop condition. */
  I2CTransaction& stop();

  /*
   * Enqueues writing the given 7 bit address, followed by one bit indicating
   * whether this is a read or write request.
   *
   * This command will expect an ACK before continuing.
   */
  I2CTransaction& write_addr(uint8_t addr, uint8_t op);

  /*
   * Enqueues one or more bytes to be written. The transaction will wait for
   * an ACK to be returned before writing the next byte.
   */
  I2CTransaction& write_ack(uint8_t data);
  template <typename... More>
  I2CTransaction& write_ack(uint8_t data, More... more) {
    write_ack(data);
    write_ack(more...);
    return *this;
  }

  /*
   * Enqueues a read of one byte into the given uint8. Responds with the given
   * ACK/NACK type.
   */
  I2CTransaction& read(uint8_t* dest, i2c_ack_type_t ack);

  /* Returns the underlying command buffer. */
  i2c_cmd_handle_t handle() { return handle_; }

  // Cannot be moved or copied, since doing so is probably an error. Pass a
  // reference instead.
  I2CTransaction(const I2CTransaction&) = delete;
  I2CTransaction& operator=(const I2CTransaction&) = delete;

 private:
  i2c_cmd_handle_t handle_;
  uint8_t* buffer_;
};

}  // namespace gay_ipod
