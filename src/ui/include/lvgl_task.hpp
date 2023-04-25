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
