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
#include "font/lv_font.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_timer.h"
#include "touchwheel.hpp"
#include "tasks.hpp"
#include "widgets/lv_label.h"

#include "display.hpp"
#include "gpio_expander.hpp"

namespace ui {

static const char* kTag = "lv_task";

auto tick_hook(TimerHandle_t xTimer) -> void {
  lv_tick_inc(1);
}

void LvglMain(std::weak_ptr<drivers::TouchWheel> weak_touch_wheel, std::weak_ptr<drivers::Display> weak_display) {
    ESP_LOGI(kTag, "init lvgl");
    lv_init();

    // LVGL has been initialised, so we can now start reporting ticks to it.
    xTimerCreate("lv_tick", pdMS_TO_TICKS(1), pdTRUE, NULL, &tick_hook);

    lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, LV_COLOR_MAKE(0xFF, 0, 0));
    // TODO: find a nice bitmap font for this display size and density.
    // lv_style_set_text_font(&style, &lv_font_montserrat_24);

    auto label = lv_label_create(NULL);
    lv_label_set_text(label, "COLOURS!!");
    lv_obj_add_style(label, &style, 0);

    lv_obj_center(label);
    lv_scr_load(label);

    while (1) {
      lv_timer_handler();
      // 30 FPS
      // TODO(jacqueline): make this dynamic
      vTaskDelay(pdMS_TO_TICKS(33));
    }
}

auto StartLvgl(std::weak_ptr<drivers::TouchWheel> touch_wheel,
               std::weak_ptr<drivers::Display> display) -> void {
  tasks::StartPersistent<tasks::Type::kUi>([=]() { LvglMain(touch_wheel, display); });
}

}  // namespace ui
