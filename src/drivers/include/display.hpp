/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>

#include "driver/spi_master.h"
#include "lvgl/lvgl.h"
#include "result.hpp"
#include "tasks.hpp"

#include "display_init.hpp"
#include "gpio_expander.hpp"

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
  static auto Create(GpioExpander* expander,
                     const displays::InitialisationData& init_data) -> Display*;

  Display(GpioExpander* gpio, spi_device_handle_t handle);
  ~Display();

  /* Driver callback invoked by LVGL when there is new data to display. */
  void OnLvglFlush(lv_disp_drv_t* disp_drv,
                   const lv_area_t* area,
                   lv_color_t* color_map);

 private:
  GpioExpander* gpio_;
  spi_device_handle_t handle_;

  std::unique_ptr<tasks::Worker> worker_task_;

  lv_disp_draw_buf_t buffers_;
  lv_disp_drv_t driver_;
  lv_disp_t* display_ = nullptr;

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
};

}  // namespace drivers
