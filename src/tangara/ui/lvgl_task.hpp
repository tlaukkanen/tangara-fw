/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <atomic>
#include <cstdbool>
#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "drivers/display.hpp"
#include "input/lvgl_input_driver.hpp"
#include "drivers/touchwheel.hpp"
#include "ui/screen.hpp"
#include "ui/themes.hpp"

namespace ui {

class UiTask {
 public:
  static auto Start() -> UiTask*;
  ~UiTask();

  auto input(std::shared_ptr<input::LvglInputDriver> input) -> void;

 private:
  UiTask();

  auto Main() -> void;

  std::shared_ptr<input::LvglInputDriver> input_;
  std::shared_ptr<Screen> current_screen_;
};

}  // namespace ui
