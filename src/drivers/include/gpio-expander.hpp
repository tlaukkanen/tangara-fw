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
  // 1 - usb interface power enable
  // 2 - display power enable
  // 3 - sd card power enable
  // 4 - charge power ok (active low)
  // 5 - sd mux switch
  // 6 - sd chip select
  // 7 - display chip select
  // All power switches low, chip selects high, active-low charge power high
  static const uint8_t kPortADefault = 0b11010001;

  // Port B:
  // 0 - 3.5mm jack detect (active low)
  // 1 - dac soft mute switch
  // 2 - GPIO
  // 3 - GPIO
  // 4 - GPIO
  // 5 - GPIO
  // 6 - GPIO
  // 7 - GPIO
  // DAC mute output low, everything else is active-low inputs.
  static const uint8_t kPortBDefault = 0b11111111;

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
    SD_CARD_POWER_ENABLE = 3,
    CHARGE_POWER_OK = 4,  // Active-low input
    SD_MUX_SWITCH = 5,
    SD_CHIP_SELECT = 6,
    DISPLAY_CHIP_SELECT = 7,

    // Port B
    PHONE_DETECT = 8,  // Active-high input
    DAC_MUTE = 9,
    GPIO_1 = 10,
    GPIO_2 = 11,
    GPIO_3 = 12,
    GPIO_4 = 13,
    GPIO_5 = 14,
    GPIO_6 = 15,
  };

  /* Pins whose access should be guarded by `cs_lock`. */
  enum ChipSelect {
    SD_CARD = SD_CHIP_SELECT,
    DISPLAY = DISPLAY_CHIP_SELECT,
  };

  /* Nicer value names for use with the SD_MUX_SWITCH pin. */
  enum SdController {
    SD_MUX_ESP = 0,
    SD_MUX_USB = 1,
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
  void set_pin(ChipSelect cs, bool value);

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

  /* Returns the mutex that must be held whilst pulling a CS pin low. */
  std::mutex& cs_mutex() { return cs_mutex_; }

  /*
   * Helper class containing an active `cs_mutex` lock. When an instance of
   * this class is destroyed (usually by falling out of scope), the associated
   * CS pin will be driven high before the lock is released.
   */
  class SpiLock {
   public:
    SpiLock(GpioExpander& gpio, ChipSelect cs);
    ~SpiLock();

    SpiLock(const SpiLock&) = delete;

   private:
    std::scoped_lock<std::mutex> lock_;
    GpioExpander& gpio_;
    ChipSelect cs_;
  };

  /*
   * Pulls the given CS pin low to signal that we are about to communicate
   * with a particular device, after acquiring a lock on `cs_mutex`. The
   * recommended way to safely interact with devices on the SPI bus is to have
   * a self-contained block like so:
   *
   * ```
   * {
   *	auto lock = AcquireSpiBus(WHATEVER);
   *  // Do some cool things here.
   * }
   * ```
   */
  SpiLock AcquireSpiBus(ChipSelect cs);

  // Not copyable or movable. There should usually only ever be once instance
  // of this class, and that instance will likely have a static lifetime.
  GpioExpander(const GpioExpander&) = delete;
  GpioExpander& operator=(const GpioExpander&) = delete;

 private:
  std::mutex cs_mutex_;
  std::atomic<uint16_t> ports_;
  std::atomic<uint16_t> inputs_;
};

}  // namespace drivers
