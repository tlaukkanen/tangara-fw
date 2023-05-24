/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lvgl_task.hpp"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/lv_disp.h"
#include "core/lv_obj.h"
#include "core/lv_obj_pos.h"
#include "core/lv_obj_tree.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "font/lv_font.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "lv_api_map.h"
#include "lvgl/lvgl.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_timer.h"
#include "tasks.hpp"
#include "touchwheel.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_label.h"

#include "display.hpp"
#include "gpio_expander.hpp"

namespace ui {

static const char* kTag = "lv_task";

void LvglMain(std::weak_ptr<drivers::TouchWheel> weak_touch_wheel,
              std::weak_ptr<drivers::Display> weak_display) {
  ESP_LOGI(kTag, "init lvgl");
  lv_init();

  std::shared_ptr<Screen> current_screen;
  auto& events = events::EventQueue::GetInstance();
  while (1) {
    while (events.ServiceUi(0)) {
    }

    std::shared_ptr<Screen> screen = UiState::current_screen();
    if (screen != current_screen && screen != nullptr) {
      current_screen = screen;
      // TODO(jacqueline): animate this sometimes
      lv_scr_load(screen->root());
    }

    lv_task_handler();
    // 30 FPS
    // TODO(jacqueline): make this dynamic
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}

auto StartLvgl(std::weak_ptr<drivers::TouchWheel> touch_wheel,
               std::weak_ptr<drivers::Display> display) -> void {
  tasks::StartPersistent<tasks::Type::kUi>(
      [=]() { LvglMain(touch_wheel, display); });
}

}  // namespace ui
