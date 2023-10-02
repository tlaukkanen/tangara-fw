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

#include "display.hpp"
#include "encoder_input.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "themes.hpp"
#include "touchwheel.hpp"

namespace ui {

class UiTask {
 public:
  static auto Start() -> UiTask*;
  ~UiTask();

  auto input(std::shared_ptr<EncoderInput> input) -> void;

 private:
  UiTask();

  auto Main() -> void;

  std::shared_ptr<EncoderInput> input_;
  std::shared_ptr<Screen> current_screen_;
};

}  // namespace ui
