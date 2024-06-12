/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <memory>

#include "driver/spi_master.h"
#include "lvgl/lvgl.h"
#include "misc/lv_types.h"
#include "result.hpp"
#include "tasks.hpp"

#include "drivers/display_init.hpp"
#include "drivers/gpios.hpp"

namespace drivers {

/*
 * LVGL display driver for ST77XX family displays.
 */
class Display {
 public:
  /*
   * Creates the display driver, and resets and reinitialises the display
   * over SPI. This never fails, since unfortunately these display don't give
   * us back any kind of signal to tell us we're actually using them correctly.
   */
  static auto Create(IGpios& expander,
                     const displays::InitialisationData& init_data) -> Display*;

  Display(IGpios& gpio, spi_device_handle_t handle);
  ~Display();

  auto SetDisplayOn(bool) -> void;
  auto SetBrightness(uint_fast8_t) -> void;

  /* Driver callback invoked by LVGL when there is new data to display. */
  void OnLvglFlush(const lv_area_t* area, uint8_t* px_map);

  // Not copyable or movable.
  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

 private:
  IGpios& gpio_;
  spi_device_handle_t handle_;

  bool first_flush_finished_;
  bool display_on_;
  uint_fast8_t brightness_;

  lv_display_t* display_ = nullptr;

  enum TransactionType {
    COMMAND = 0,
    DATA = 1,
  };

  void SendInitialisationSequence(const uint8_t* data);

  void SendCommandWithData(uint8_t command, const uint8_t* data, size_t length);
  void SendCmd(const uint8_t* data, size_t length);
  void SendData(const uint8_t* data, size_t length);

  void SendTransaction(TransactionType type,
                       const uint8_t* data,
                       size_t length);

  auto SetDutyCycle(uint_fast8_t, bool) -> void;
};

}  // namespace drivers
