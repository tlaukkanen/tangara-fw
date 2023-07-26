/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
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
class IGpios {
 public:
  virtual ~IGpios() {}

  /* Maps each pin of the expander to its number in a `pack`ed uint16. */
  enum class Pin {
    // Port A
    kSdMuxSwitch = 0,
    kSdMuxDisable = 1,
    kKeyUp = 2,
    kKeyDown = 3,
    kKeyLock = 4,
    kDisplayEnable = 5,
    // 6 is unused
    kSdPowerEnable = 7,

    // Port B
    kPhoneDetect = 8,
    kAmplifierEnable = 9,
    kSdCardDetect = 10,
    // 11 through 15 are unused
  };

  /* Nicer value names for use with kSdMuxSwitch. */
  enum SdController {
    SD_MUX_ESP = 0,
    SD_MUX_SAMD = 1,
  };

  /*
   * Sets a single specific pin to the given value. `true` corresponds to
   * HIGH, and `false` corresponds to LOW.
   *
   * This function will block until the pin has changed level.
   */
  virtual auto WriteSync(Pin, bool) -> bool = 0;

  /*
   * Returns the most recently cached value of the given pin.
   */
  virtual auto Get(Pin) const -> bool = 0;
};

class Gpios : public IGpios {
 public:
  static auto Create() -> Gpios*;
  ~Gpios();

  /*
   * Sets a single specific pin to the given value. `true` corresponds to
   * HIGH, and `false` corresponds to LOW.
   *
   * Calls to this method will be buffered in memory until a call to `Write()`
   * is made.
   */
  auto WriteSync(Pin, bool) -> bool override;

  virtual auto WriteBuffered(Pin, bool) -> void;

  /**
   * Sets the ports on the GPIO expander to the values currently represented
   * in `ports`.
   */
  auto Flush(void) -> bool;

  auto Get(Pin) const -> bool override;

  /**
   * Reads from the GPIO expander, populating `inputs` with the most recent
   * values.
   */
  auto Read(void) -> bool;

  auto InstallReadPendingISR() -> void;
  auto IsReadPending() -> SemaphoreHandle_t { return read_pending_; }

  // Not copyable or movable. There should usually only ever be once instance
  // of this class, and that instance will likely have a static lifetime.
  Gpios(const Gpios&) = delete;
  Gpios& operator=(const Gpios&) = delete;

 private:
  Gpios();

  std::atomic<uint16_t> ports_;
  std::atomic<uint16_t> inputs_;

  SemaphoreHandle_t read_pending_;
};

}  // namespace drivers
