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
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "themes.hpp"
#include "touchwheel.hpp"
#include "wheel_encoder.hpp"

namespace ui {

class UiTask {
 public:
  static auto Start() -> UiTask*;

  ~UiTask();

  // FIXME: Once we have more input devices, this function should accept a more
  // generic interface.
  auto SetInputDevice(std::shared_ptr<TouchWheelEncoder> dev) -> void;

 private:
  UiTask();

  auto Main() -> void;

  std::shared_ptr<TouchWheelEncoder> input_device_;
  std::shared_ptr<Screen> current_screen_;

  std::atomic<bool> quit_;
  SemaphoreHandle_t frame_semaphore_;
  TimerHandle_t frame_timer_;
};

}  // namespace ui
