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
#include "relative_wheel.hpp"
#include "touchwheel.hpp"
#include "themes.hpp"

namespace ui {

auto StartLvgl(std::weak_ptr<drivers::RelativeWheel> touch_wheel,
               std::weak_ptr<drivers::Display> display) -> void;

}  // namespace ui
