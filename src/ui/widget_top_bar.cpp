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

  themes::Theme::instance()->ApplyStyle(container_, themes::Style::kTopBar);
}

}  // namespace widgets
}  // namespace ui
