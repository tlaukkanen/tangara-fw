/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/gpios.hpp"

#include <cstdint>

#include "assert.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "hal/gpio_types.h"

#include "drivers/i2c.hpp"

namespace drivers {

static const uint8_t kPca8575Address = 0x20;

// Port A:
// 0 - sd card mux switch
// 1 - sd card mux enable (active low)
// 2 - key up
// 3 - key down
// 4 - key lock
// 5 - display reset (active low)
// 6 - NC
// 7 - sd card power
// Default to SD card off, inputs high, display running
static const uint8_t kPortADefault = 0b00111110;

// Port B:
// 0 - 3.5mm jack detect (active low)
// 1 - headphone amp power enable
// 2 - sd card detect
// 3 - amplifier unmute (revisions < r8)
// 4 - amplifier mute (revisions >= r8)
// 5 - NC
// 6 - NC
// 7 - NC
// Default inputs high, amp off.
static const uint8_t kPortBDefault = 0b00011111;

/*
 * Convenience mehod for packing the port a and b bytes into a single 16 bit
 * value.
 */
constexpr uint16_t pack(uint8_t a, uint8_t b) {
  return ((uint16_t)b) << 8 | a;
}

/*
 * Convenience mehod for unpacking the result of `pack` back into two single
 * byte port datas.
 */
constexpr std::pair<uint8_t, uint8_t> unpack(uint16_t ba) {
  return std::pair((uint8_t)ba, (uint8_t)(ba >> 8));
}

static constexpr gpio_num_t kIntPin = GPIO_NUM_34;

auto Gpios::Create(bool invert_lock) -> Gpios* {
  Gpios* instance = new Gpios(invert_lock);
  // Read and write initial values on initialisation so that we do not have a
  // strange partially-initialised state.
  if (!instance->Flush() || !instance->Read()) {
    return nullptr;
  }
  return instance;
}

Gpios::Gpios(bool invert_lock)
    : ports_(pack(kPortADefault, kPortBDefault)),
      inputs_(0),
      invert_lock_switch_(invert_lock) {
  gpio_set_direction(kIntPin, GPIO_MODE_INPUT);
}

Gpios::~Gpios() {}

auto Gpios::WriteBuffered(Pin pin, bool value) -> void {
  if (value) {
    ports_ |= (1 << static_cast<int>(pin));
  } else {
    ports_ &= ~(1 << static_cast<int>(pin));
  }
}

auto Gpios::WriteSync(Pin pin, bool value) -> bool {
  WriteBuffered(pin, value);
  return Flush();
}

auto Gpios::Flush() -> bool {
  std::pair<uint8_t, uint8_t> ports_ab = unpack(ports_);

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPca8575Address, I2C_MASTER_WRITE)
      .write_ack(ports_ab.first, ports_ab.second)
      .stop();

  return transaction.Execute() == ESP_OK;
}

auto Gpios::Get(Pin pin) const -> bool {
  return (inputs_ & (1 << static_cast<int>(pin))) > 0;
}

auto Gpios::IsLocked() const -> bool {
  bool pin = Get(Pin::kKeyLock);
  if (invert_lock_switch_) {
    return pin;
  } else {
    return !pin;
  }
}

auto Gpios::Read() -> bool {
  uint8_t input_a, input_b;

  I2CTransaction transaction;
  transaction.start()
      .write_addr(kPca8575Address, I2C_MASTER_READ)
      .read(&input_a, I2C_MASTER_ACK)
      .read(&input_b, I2C_MASTER_LAST_NACK)
      .stop();

  esp_err_t ret = transaction.Execute();
  if (ret != ESP_OK) {
    return false;
  }
  inputs_ = pack(input_a, input_b);
  return true;
}

}  // namespace drivers
