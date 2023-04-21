#pragma once

#include <atomic>
#include <cstdbool>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_expander.hpp"

namespace ui {

auto StartLvgl(drivers::GpioExpander* gpios,
               std::atomic<bool>* quit,
               TaskHandle_t* handle) -> bool;

}  // namespace ui
