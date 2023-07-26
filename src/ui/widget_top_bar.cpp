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
#include "font/lv_symbol_def.h"
#include "font_symbols.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_img.h"
#include "widgets/lv_label.h"

namespace ui {
namespace widgets {

static void back_click_cb(lv_event_t* ev) {
  events::Ui().Dispatch(internal::BackPressed{});
}

TopBar::TopBar(lv_obj_t* parent, const Configuration& config) {
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, lv_pct(100), 14);
  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_END);

  if (config.show_back_button) {
    back_button_ = lv_btn_create(container_);
    lv_obj_set_size(back_button_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_t* button_icon = lv_label_create(back_button_);
    lv_label_set_text(button_icon, "");
    lv_obj_set_style_text_font(button_icon, &font_symbols, 0);
    lv_obj_add_event_cb(back_button_, back_click_cb, LV_EVENT_CLICKED, NULL);
  } else {
    back_button_ = nullptr;
  }

  title_ = lv_label_create(container_);
  lv_label_set_text(title_, config.title.c_str());
  lv_obj_set_flex_grow(title_, 1);

  playback_ = lv_label_create(container_);
  lv_label_set_text(playback_, "");

  battery_ = lv_label_create(container_);
  lv_label_set_text(battery_, "");
}

auto TopBar::Update(const State& state) -> void {
  switch (state.playback_state) {
    case PlaybackState::kIdle:
      lv_label_set_text(playback_, "");
      break;
    case PlaybackState::kPaused:
      lv_label_set_text(playback_, "");
      break;
    case PlaybackState::kPlaying:
      lv_label_set_text(playback_, "");
      break;
  }

  if (state.battery_percent >= 95) {
    lv_label_set_text(battery_, "");
  } else if (state.battery_percent >= 70) {
    lv_label_set_text(battery_, "");
  } else if (state.battery_percent >= 40) {
    lv_label_set_text(battery_, "");
  } else if (state.battery_percent >= 10) {
    lv_label_set_text(battery_, "");
  } else {
    lv_label_set_text(battery_, "");
  }
}

}  // namespace widgets
}  // namespace ui
