/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui/lvgl_task.hpp"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/lv_disp.h"
#include "core/lv_group.h"
#include "core/lv_indev.h"
#include "core/lv_obj.h"
#include "core/lv_obj_pos.h"
#include "core/lv_obj_tree.h"
#include "drivers/touchwheel.hpp"
#include "esp_log.h"
#include "events/event_queue.hpp"
#include "extra/themes/basic/lv_theme_basic.h"
#include "font/lv_font.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "hal/gpio_types.h"
#include "hal/lv_hal_indev.h"
#include "hal/spi_types.h"
#include "input/lvgl_input_driver.hpp"
#include "lua.h"
#include "lv_api_map.h"
#include "lvgl/lvgl.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_timer.h"
#include "tasks.hpp"
#include "ui/modal.hpp"
#include "ui/ui_fsm.hpp"
#include "widgets/lv_label.h"

#include "drivers/display.hpp"
#include "drivers/gpios.hpp"

namespace ui {

[[maybe_unused]] static const char* kTag = "ui_task";

UiTask::UiTask() {}

UiTask::~UiTask() {
  assert(false);
}

auto UiTask::Main() -> void {
  ESP_LOGI(kTag, "start ui task");
  lv_group_t* current_group = nullptr;
  auto* events = events::queues::Ui();
  while (true) {
    while (events->Service(0)) {
    }

    std::shared_ptr<Screen> screen = UiState::current_screen();
    if (screen != current_screen_ && screen != nullptr) {
      lv_scr_load(screen->root());
      if (input_) {
        lv_indev_set_group(input_->registration(), screen->group());
      }
      current_screen_ = screen;
    }

    if (input_ && current_screen_->group() != current_group) {
      current_group = current_screen_->group();
      lv_indev_set_group(input_->registration(), current_group);
    }

    TickType_t delay = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(std::clamp<TickType_t>(delay, 0, 100)));
  }
}

auto UiTask::input(std::shared_ptr<input::LvglInputDriver> input) -> void {
  input_ = input;
}

auto UiTask::Start() -> UiTask* {
  UiTask* ret = new UiTask();
  tasks::StartPersistent<tasks::Type::kUi>([=]() { ret->Main(); });
  return ret;
}

}  // namespace ui
