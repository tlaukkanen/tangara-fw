/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_playing.hpp"

#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "esp_log.h"
#include "extra/layouts/flex/lv_flex.h"
#include "extra/layouts/grid/lv_grid.h"
#include "font/lv_symbol_def.h"
#include "lvgl.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "track.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_bar.h"
#include "widgets/lv_btn.h"
#include "widgets/lv_img.h"
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

static lv_style_t scrubber_style;

auto info_label(lv_obj_t* parent) -> lv_obj_t* {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_center(label);
  return label;
}

auto control_button(lv_obj_t* parent, char* icon) -> lv_obj_t* {
  lv_obj_t* button = lv_img_create(parent);
  lv_obj_set_size(button, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_img_set_src(button, icon);
  return button;
}

auto next_up_label(lv_obj_t* parent, const std::string& text) -> lv_obj_t* {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text.c_str());
  lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  return label;
}

Playing::Playing(database::Track track) : track_(track) {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_obj_set_size(root_, lv_pct(100), lv_pct(200));
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  lv_obj_t* above_fold_container = lv_obj_create(root_);
  lv_obj_set_layout(above_fold_container, LV_LAYOUT_FLEX);
  lv_obj_set_size(above_fold_container, lv_pct(100), lv_disp_get_ver_res(NULL));
  lv_obj_set_flex_flow(above_fold_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(above_fold_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

  lv_obj_t* info_container = lv_obj_create(above_fold_container);
  lv_obj_set_layout(info_container, LV_LAYOUT_FLEX);
  lv_obj_set_size(info_container, lv_pct(100), 80);
  lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

  artist_label_ = info_label(info_container);
  album_label_ = info_label(info_container);
  title_label_ = info_label(info_container);

  scrubber_ = lv_bar_create(above_fold_container);
  lv_obj_set_size(scrubber_, lv_pct(100), 5);
  lv_bar_set_range(scrubber_, 0, 100);
  lv_bar_set_value(scrubber_, 0, LV_ANIM_OFF);

  lv_style_init(&scrubber_style);
  lv_style_set_bg_color(&scrubber_style, lv_color_black());
  lv_obj_add_style(scrubber_, &scrubber_style, LV_PART_INDICATOR);

  lv_obj_t* controls_container = lv_obj_create(above_fold_container);
  lv_obj_set_size(controls_container, lv_pct(100), 20);
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  play_pause_control_ = control_button(controls_container, LV_SYMBOL_PLAY);
  lv_group_add_obj(group_, play_pause_control_);
  lv_group_add_obj(group_, control_button(controls_container, LV_SYMBOL_PREV));
  lv_group_add_obj(group_, control_button(controls_container, LV_SYMBOL_NEXT));
  lv_group_add_obj(group_,
                   control_button(controls_container, LV_SYMBOL_SHUFFLE));
  lv_group_add_obj(group_, control_button(controls_container, LV_SYMBOL_LOOP));

  lv_obj_t* next_up_header = lv_obj_create(above_fold_container);
  lv_obj_set_size(next_up_header, lv_pct(100), 15);
  lv_obj_set_flex_flow(next_up_header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(next_up_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_END);

  lv_obj_t* next_up_title = lv_label_create(next_up_header);
  lv_label_set_text(next_up_title, "Next up...");
  lv_obj_set_height(next_up_title, lv_pct(100));
  lv_obj_set_flex_grow(next_up_title, 1);

  lv_obj_t* next_up_hint = lv_label_create(next_up_header);
  lv_label_set_text(next_up_hint, LV_SYMBOL_DOWN);
  lv_obj_set_size(next_up_hint, LV_SIZE_CONTENT, lv_pct(100));

  next_up_container_ = lv_obj_create(root_);
  lv_obj_set_layout(next_up_container_, LV_LAYOUT_FLEX);
  lv_obj_set_width(next_up_container_, lv_pct(100));
  lv_obj_set_flex_grow(next_up_container_, 1);
  lv_obj_set_flex_flow(next_up_container_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(next_up_container_, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

  BindTrack(track);
}

Playing::~Playing() {}

auto Playing::BindTrack(database::Track t) -> void {
  track_ = t;

  lv_label_set_text(artist_label_,
                    t.tags().at(database::Tag::kArtist).value_or("").c_str());
  lv_label_set_text(album_label_,
                    t.tags().at(database::Tag::kAlbum).value_or("").c_str());
  lv_label_set_text(title_label_, t.TitleOrFilename().c_str());

  // TODO.
  lv_bar_set_range(scrubber_, 0, 0);
  lv_bar_set_value(scrubber_, 0, LV_ANIM_OFF);
}

auto Playing::UpdateTime(uint32_t time) -> void {
  lv_bar_set_value(scrubber_, time, LV_ANIM_OFF);
}

auto Playing::UpdateNextUp(std::vector<database::Track> tracks) -> void {
  // TODO(jacqueline): Do a proper diff to maintain selection.
  int children = lv_obj_get_child_cnt(next_up_container_);
  while (children > 0) {
    lv_obj_del(lv_obj_get_child(next_up_container_, 0));
    children--;
  }

  next_tracks_ = tracks;
  for (const auto &track : next_tracks_) {
    lv_group_add_obj(group_, next_up_label(next_up_container_, track.TitleOrFilename()));
  }
}

}  // namespace screens
}  // namespace ui
