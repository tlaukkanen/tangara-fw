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

#include "display.hpp"
#include "touchwheel.hpp"

namespace ui {

auto StartLvgl(std::weak_ptr<drivers::TouchWheel> touch_wheel,
               std::weak_ptr<drivers::Display> display) -> void;

}  // namespace ui
