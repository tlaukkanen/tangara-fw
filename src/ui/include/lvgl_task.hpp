/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <atomic>
#include <cstdbool>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_cache.hpp"

namespace ui {

auto StartLvgl(drivers::DriverCache* drivers,
               std::atomic<bool>* quit,
               TaskHandle_t* handle) -> bool;

}  // namespace ui
