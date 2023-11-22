/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "widget_top_bar.hpp"
#include "battery.hpp"
#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "event_queue.hpp"
#include "extra/layouts/flex/lv_flex.h"
#include "font/lv_symbol_def.h"
#include "model_top_bar.hpp"
#include "themes.hpp"
#include "track.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_img.h"
#include "widgets/lv_label.h"

LV_IMG_DECLARE(kIconBluetooth);
LV_IMG_DECLARE(kIconPlay);
LV_IMG_DECLARE(kIconPause);
LV_IMG_DECLARE(kIconBatteryEmpty);
LV_IMG_DECLARE(kIconBattery20);
LV_IMG_DECLARE(kIconBattery40);
LV_IMG_DECLARE(kIconBattery60);
LV_IMG_DECLARE(kIconBattery80);
LV_IMG_DECLARE(kIconBatteryFull);

namespace ui {
namespace widgets {

static void back_click_cb(lv_event_t* ev) {
  events::Ui().Dispatch(internal::BackPressed{});
}

TopBar::TopBar(lv_obj_t* parent,
               const Configuration& config,
               models::TopBar& model) {
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, lv_pct(100), 20);
  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(container_, 5, LV_PART_MAIN);

  if (config.show_back_button) {
    back_button_ = lv_btn_create(container_);
    lv_obj_set_size(back_button_, LV_SIZE_CONTENT, 12);
    lv_obj_t* button_icon = lv_label_create(back_button_);
    lv_label_set_text(button_icon, "<");
    lv_obj_add_event_cb(back_button_, back_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(button_icon);
  } else {
    back_button_ = nullptr;
  }

  lv_obj_t* title_ = lv_label_create(container_);
  lv_obj_set_height(title_, 17);
  lv_obj_set_flex_grow(title_, 1);

  lv_label_set_text(title_, config.title.c_str());
  lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);

  lv_obj_t* playback = lv_img_create(container_);

  bindings_.push_back(model.is_playing.onChangedAndNow([=](bool is_playing) {
    lv_img_set_src(playback, is_playing ? &kIconPlay : &kIconPause);
  }));
  bindings_.push_back(model.current_track.onChangedAndNow(
      [=](const std::optional<database::TrackId>& id) {
        if (id) {
          lv_obj_clear_flag(playback, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(playback, LV_OBJ_FLAG_HIDDEN);
        }
      }));

  lv_obj_t* battery = lv_img_create(container_);
  lv_obj_t* charging = lv_label_create(container_);

  bindings_.push_back(model.battery_state.onChangedAndNow(
      [=](const battery::Battery::BatteryState& state) {
        if (state.is_charging) {
          lv_label_set_text(charging, "+");
        } else {
          lv_label_set_text(charging, "");
        }

        if (state.percent >= 95) {
          lv_img_set_src(battery, &kIconBatteryFull);
        } else if (state.percent >= 75) {
          lv_img_set_src(battery, &kIconBattery80);
        } else if (state.percent >= 55) {
          lv_img_set_src(battery, &kIconBattery60);
        } else if (state.percent >= 35) {
          lv_img_set_src(battery, &kIconBattery40);
        } else if (state.percent >= 15) {
          lv_img_set_src(battery, &kIconBattery20);
        } else {
          lv_img_set_src(battery, &kIconBatteryEmpty);
        }
      }));

  themes::Theme::instance()->ApplyStyle(container_, themes::Style::kTopBar);
}

}  // namespace widgets
}  // namespace ui
