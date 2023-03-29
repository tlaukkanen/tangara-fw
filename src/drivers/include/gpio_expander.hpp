#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <tuple>
#include <utility>

#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

namespace drivers {

/**
 * Wrapper for interfacing with the PCA8575 GPIO expander. Includes basic
 * low-level pin setting methods, as well as higher level convenience functions
 * for reading, writing, and atomically interacting with the SPI chip select
 * pins.
 *
 * Each method of this class can be called safely from any thread, and all
 * updates are guaranteed to be atomic. Any access to chip select related pins
 * should be done whilst holding `cs_lock` (preferably via the helper methods).
 */
class GpioExpander {
 public:
  GpioExpander();
  ~GpioExpander();

  static const uint8_t kPca8575Address = 0x20;
  static const uint8_t kPca8575Timeout = pdMS_TO_TICKS(100);

  // Port A:
  // 0 - sd card mux switch
  // 1 - sd card mux enable (active low)
  // 2 - key up
  // 3 - key down
  // 4 - key lock
  // 5 - display reset
  // 6 - NC
  // 7 - sd card power (active low)
  // Default to SD card off, inputs high.
  static const uint8_t kPortADefault = 0b10011110;

  // Port B:
  // 0 - trs output enable
  // 1 - 3.5mm jack detect (active low)
  // 2 - NC
  // 3 - NC
  // 4 - NC
  // 5 - NC
  // 6 - NC
  // 7 - NC
  // Default input high, trs output low
  static const uint8_t kPortBDefault = 0b00000010;

  /*
   * Convenience mehod for packing the port a and b bytes into a single 16 bit
   * value.
   */
  static uint16_t pack(uint8_t a, uint8_t b) { return ((uint16_t)b) << 8 | a; }

  /*
   * Convenience mehod for unpacking the result of `pack` back into two single
   * byte port datas.
   */
  static std::pair<uint8_t, uint8_t> unpack(uint16_t ba) {
    return std::pair((uint8_t)ba, (uint8_t)(ba >> 8));
  }

  /*
   * Convenience function for running some arbitrary pin writing code, then
   * flushing a `Write()` to the expander. Example usage:
   *
   * ```
   * gpio_.with([&](auto& gpio) {
   *	gpio.set_pin(AUDIO_POWER_ENABLE, true);
   * });
   * ```
   */
  void with(std::function<void(GpioExpander&)> f);

  /**
   * Sets the ports on the GPIO expander to the values currently represented
   * in `ports`.
   */
  esp_err_t Write(void);

  /**
   * Reads from the GPIO expander, populating `inputs` with the most recent
   * values.
   */
  esp_err_t Read(void);

  /* Maps each pin of the expander to its number in a `pack`ed uint16. */
  enum Pin {
    // Port A
    SD_MUX_SWITCH = 0,
    SD_MUX_EN_ACTIVE_LOW = 1,
    KEY_UP = 2,
    KEY_DOWN = 3,
    KEY_LOCK = 4,
    DISPLAY_RESET = 5,
    // UNUSED = 6,
    SD_CARD_POWER_ENABLE = 7,

    // Port B
    AMP_EN = 8,
    PHONE_DETECT = 9,
    // UNUSED = 10,
    // UNUSED = 11,
    // UNUSED = 12,
    // UNUSED = 13,
    // UNUSED = 14,
    // UNUSED = 15,
  };

  /* Nicer value names for use with the SD_MUX_SWITCH pin. */
  enum SdController {
    SD_MUX_ESP = 0,
    SD_MUX_SAMD = 1,
  };

  /**
   * Returns the current driven status of each of the ports. The first byte is
   * port a, and the second byte is port b.
   */
  std::atomic<uint16_t>& ports() { return ports_; }

  /*
   * Sets a single specific pin to the given value. `true` corresponds to
   * HIGH, and `false` corresponds to LOW.
   *
   * Calls to this method will be buffered in memory until a call to `Write()`
   * is made.
   */
  void set_pin(Pin pin, bool value);

  /**
   * Returns the input status of each of the ports. The first byte is port a,
   * and the second byte is port b.
   */
  const std::atomic<uint16_t>& inputs() const { return inputs_; }

  /* Returns the most recently cached value of the given pin. Only valid for
   * pins used as inputs; to check what value we're driving a pin, use
   * `ports()`.
   */
  bool get_input(Pin pin) const;

  // Not copyable or movable. There should usually only ever be once instance
  // of this class, and that instance will likely have a static lifetime.
  GpioExpander(const GpioExpander&) = delete;
  GpioExpander& operator=(const GpioExpander&) = delete;

 private:
  std::atomic<uint16_t> ports_;
  std::atomic<uint16_t> inputs_;
};

}  // namespace drivers
