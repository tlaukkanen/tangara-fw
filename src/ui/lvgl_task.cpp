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
#include "wheel_encoder.hpp"
#include "widgets/lv_label.h"

#include "display.hpp"
#include "gpios.hpp"

namespace ui {

static const char* kTag = "ui_task";
static const TickType_t kMaxFrameRate = pdMS_TO_TICKS(100);

static auto next_frame(TimerHandle_t t) {
  SemaphoreHandle_t sem =
      reinterpret_cast<SemaphoreHandle_t>(pvTimerGetTimerID(t));
  xSemaphoreGive(sem);
}

UiTask::UiTask()
    : quit_(false),
      frame_semaphore_(xSemaphoreCreateBinary()),
      frame_timer_(xTimerCreate("ui_frame",
                                kMaxFrameRate,
                                pdTRUE,
                                frame_semaphore_,
                                next_frame)) {
  xTimerStart(frame_timer_, portMAX_DELAY);
}

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
      if (input_device_) {
        lv_indev_set_group(input_device_->registration(), screen->group());
      }
      current_screen_ = screen;
    }

    if (input_device_ && current_screen_->group() != current_group) {
      current_group = current_screen_->group();
      lv_indev_set_group(input_device_->registration(), current_group);
    }

    if (current_screen_) {
      current_screen_->Tick();
    }

    lv_task_handler();

    // Wait for the signal to loop again.
    xSemaphoreTake(frame_semaphore_, portMAX_DELAY);
  }
}

auto UiTask::SetInputDevice(std::shared_ptr<TouchWheelEncoder> dev) -> void {
  input_device_ = std::move(dev);
  if (current_screen_ && input_device_) {
    lv_indev_set_group(input_device_->registration(), current_screen_->group());
  }
}

auto UiTask::Start() -> UiTask* {
  UiTask* ret = new UiTask();
  tasks::StartPersistent<tasks::Type::kUi>(0, [=]() { ret->Main(); });
  return ret;
}

}  // namespace ui
