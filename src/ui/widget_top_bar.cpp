/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "widget_top_bar.hpp"
#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "event_queue.hpp"
#include "extra/layouts/flex/lv_flex.h"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_img.h"

namespace ui {
namespace widgets {

static void back_click_cb(lv_event_t* ev) {
  events::Dispatch<internal::BackPressed, UiState>({});
}

TopBar::TopBar(lv_obj_t* parent, lv_group_t* group) {
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, lv_pct(100), 14);
  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_END);

  back_button_ = lv_btn_create(container_);
  lv_obj_set_size(back_button_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_t* button_icon = lv_label_create(back_button_);
  lv_label_set_text(button_icon, "<");

  lv_group_add_obj(group, back_button_);
  lv_obj_add_event_cb(back_button_, back_click_cb, LV_EVENT_CLICKED, NULL);

  title_ = lv_label_create(container_);
  lv_label_set_text(title_, "");
  lv_obj_set_flex_grow(title_, 1);

  lv_obj_t* playback_label = lv_label_create(container_);
  lv_label_set_text(playback_label, LV_SYMBOL_PAUSE);

  lv_obj_t* battery_label = lv_label_create(container_);
  lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_2);
}

auto TopBar::set_title(const std::string& new_title) -> void {
  lv_label_set_text(title_, new_title.c_str());
}

}  // namespace widgets
}  // namespace ui
