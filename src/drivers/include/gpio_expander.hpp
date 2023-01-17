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
  static const uint8_t kPca8575Timeout = 100 / portTICK_RATE_MS;

  // Port A:
  // 0 - audio power enable
  // 1 - usb interface power enable (active low)
  // 2 - display power enable
  // 3 - touchpad power enable
  // 4 - sd card power enable
  // 5 - sd mux switch
  // 6 - LDO enable
  // 7 - charge power ok (active low)
  // All power switches low, sd mux pointing away from us, inputs high.
  static const uint8_t kPortADefault = 0b10000010;

  // Port B:
  // 0 - 3.5mm jack detect (active low)
  // 1 - unused
  // 2 - volume up
  // 3 - volume down
  // 4 - lock switch
  // 5 - touchpad interupt
  // 6 - display DR
  // 7 - display LED
  // Inputs all high, all others low.
  static const uint8_t kPortBDefault = 0b00111101;

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
    AUDIO_POWER_ENABLE = 0,
    USB_INTERFACE_POWER_ENABLE = 1,
    DISPLAY_POWER_ENABLE = 2,
    TOUCHPAD_POWER_ENABLE = 3,
    SD_CARD_POWER_ENABLE = 4,
    SD_MUX_SWITCH = 5,
    LDO_ENABLE = 6,
    CHARGE_POWER_OK = 7,  // Active-low input

    // Port B
    PHONE_DETECT = 8,  // Active-high input
    //UNUSED = 9,
    VOL_UP = 10,
    VOL_DOWN = 11,
    LOCK = 12,
    TOUCHPAD_INT = 13,
    DISPLAY_DR = 14,
    DISPLAY_LED = 15,
  };

  /* Nicer value names for use with the SD_MUX_SWITCH pin. */
  enum SdController {
    SD_MUX_ESP = 1,
    SD_MUX_USB = 0,
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
