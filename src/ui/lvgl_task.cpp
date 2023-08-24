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
#include "core/lv_group.h"
#include "core/lv_indev.h"
#include "core/lv_obj.h"
#include "core/lv_obj_pos.h"
#include "core/lv_obj_tree.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "extra/themes/basic/lv_theme_basic.h"
#include "font/lv_font.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "hal/gpio_types.h"
#include "hal/lv_hal_indev.h"
#include "hal/spi_types.h"
#include "lv_api_map.h"
#include "lvgl/lvgl.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_timer.h"
#include "relative_wheel.hpp"
#include "tasks.hpp"
#include "touchwheel.hpp"
#include "ui_fsm.hpp"
#include "wheel_encoder.hpp"
#include "widgets/lv_label.h"

#include "display.hpp"
#include "gpios.hpp"

namespace ui {

static const char* kTag = "lv_task";
static const TickType_t kMaxFrameRate = pdMS_TO_TICKS(66);

static int sTimerId;
static SemaphoreHandle_t sFrameSemaphore;

auto next_frame(TimerHandle_t) {
  xSemaphoreGive(sFrameSemaphore);
}

void LvglMain(std::weak_ptr<drivers::RelativeWheel> weak_touch_wheel,
              std::weak_ptr<drivers::Display> weak_display) {
  ESP_LOGI(kTag, "init lvgl");
  lv_init();

  sFrameSemaphore = xSemaphoreCreateBinary();
  auto timer =
      xTimerCreate("lvgl_frame", kMaxFrameRate, pdTRUE, &sTimerId, next_frame);
  xTimerStart(timer, portMAX_DELAY);

  lv_theme_t* base_theme = lv_theme_basic_init(NULL);
  lv_disp_set_theme(NULL, base_theme);
  static themes::Theme sTheme{};
  sTheme.Apply();

  TouchWheelEncoder encoder(weak_touch_wheel);

  std::shared_ptr<Screen> current_screen;
  auto* events = events::queues::Ui();
  while (1) {
    while (events->Service(0)) {
    }

    std::shared_ptr<Screen> screen = UiState::current_screen();
    if (screen != current_screen && screen != nullptr) {
      // TODO(jacqueline): animate this sometimes
      lv_scr_load(screen->root());
      lv_indev_set_group(encoder.registration(), screen->group());
      current_screen = screen;
    }

    if (current_screen) {
      current_screen->Tick();
    }

    lv_task_handler();

    // Wait for the signal to loop again.
    xSemaphoreTake(sFrameSemaphore, portMAX_DELAY);
  }
}

auto StartLvgl(std::weak_ptr<drivers::RelativeWheel> touch_wheel,
               std::weak_ptr<drivers::Display> display) -> void {
  tasks::StartPersistent<tasks::Type::kUi>(
      [=]() { LvglMain(touch_wheel, display); });
}

}  // namespace ui
