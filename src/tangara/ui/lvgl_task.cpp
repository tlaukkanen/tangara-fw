/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui/lvgl_task.hpp"

#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_pos.h"
#include "core/lv_obj_tree.h"
#include "core/lv_refr.h"
#include "display/lv_display.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "lua.h"
#include "lvgl/lvgl.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_timer.h"

#include "drivers/display.hpp"
#include "events/event_queue.hpp"
#include "input/lvgl_input_driver.hpp"
#include "tasks.hpp"
#include "ui/ui_fsm.hpp"

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
      lv_screen_load(screen->root());
      if (input_) {
        input_->setGroup(screen->group());
      }
      current_screen_ = screen;
    }

    if (input_ && current_screen_ &&
        current_screen_->group() != current_group) {
      current_group = current_screen_->group();
      input_->setGroup(current_group);
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
