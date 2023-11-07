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
#include "modal.hpp"
#include "relative_wheel.hpp"
#include "tasks.hpp"
#include "touchwheel.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_label.h"

#include "display.hpp"
#include "gpios.hpp"

namespace ui {

[[maybe_unused]] static const char* kTag = "ui_task";

static auto group_focus_cb(lv_group_t *group) -> void;

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
      lv_group_set_focus_cb(current_group, &group_focus_cb);
    }

    if (current_screen_) {
      current_screen_->Tick();
    }

    TickType_t delay = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(std::clamp<TickType_t>(delay, 0, 100)));
  }
}

auto UiTask::input(std::shared_ptr<EncoderInput> input) -> void {
  assert(current_screen_);
  input_ = input;
  lv_indev_set_group(input_->registration(), current_screen_->group());
}

auto UiTask::Start() -> UiTask* {
  UiTask* ret = new UiTask();
  tasks::StartPersistent<tasks::Type::kUi>([=]() { ret->Main(); });
  return ret;
}

static auto group_focus_cb(lv_group_t *group) -> void {
  // TODO(robin): we probably want to vary this, configure this, etc
  events::System().Dispatch(system_fsm::HapticTrigger{
    .effect = drivers::Haptics::Effect::kMediumClick1_100Pct,
  });
}

}  // namespace ui
