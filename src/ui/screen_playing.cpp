/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_playing.hpp"
#include <sys/_stdint.h>
#include <memory>

#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "core/lv_obj_scroll.h"
#include "core/lv_obj_tree.h"
#include "database.hpp"
#include "esp_log.h"
#include "extra/layouts/flex/lv_flex.h"
#include "extra/layouts/grid/lv_grid.h"
#include "font/lv_font.h"
#include "font/lv_symbol_def.h"
#include "future_fetcher.hpp"
#include "lvgl.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "future_fetcher.hpp"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_anim.h"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "misc/lv_txt.h"
#include "track.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_btn.h"
#include "widgets/lv_img.h"
#include "widgets/lv_label.h"

LV_FONT_DECLARE(font_symbols);

namespace ui {
namespace screens {

static constexpr std::size_t kMaxUpcoming = 10;

static void above_fold_focus_cb(lv_event_t* ev) {
  if (ev->user_data == NULL) {
    return;
  }
  Playing* instance = reinterpret_cast<Playing*>(ev->user_data);
  instance->OnFocusAboveFold();
}

static void below_fold_focus_cb(lv_event_t* ev) {
  if (ev->user_data == NULL) {
    return;
  }
  Playing* instance = reinterpret_cast<Playing*>(ev->user_data);
  instance->OnFocusBelowFold();
}

static lv_style_t scrubber_style;

auto info_label(lv_obj_t* parent) -> lv_obj_t* {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);

  lv_obj_set_style_bg_color(label, lv_color_black(), LV_STATE_FOCUSED);
  return label;
}

auto Playing::control_button(lv_obj_t* parent, char* icon) -> lv_obj_t* {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_size(button, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_add_event_cb(button, above_fold_focus_cb, LV_EVENT_FOCUSED, this);

  lv_obj_t* icon_obj = lv_img_create(button);
  lv_img_set_src(icon_obj, icon);
  lv_obj_set_style_text_font(icon_obj, &font_symbols, 0);

  return button;
}

auto Playing::next_up_label(lv_obj_t* parent, const std::string& text)
    -> lv_obj_t* {
  lv_obj_t* button = lv_list_add_btn(parent, NULL, text.c_str());
  lv_obj_add_event_cb(button, below_fold_focus_cb, LV_EVENT_FOCUSED, this);
  lv_group_add_obj(group_, button);
  return button;
}

Playing::Playing(std::weak_ptr<database::Database> db, audio::TrackQueue* queue)
    : db_(db),
      queue_(queue),
      track_(),
      next_tracks_(),
      new_track_(),
      new_next_tracks_() {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_group_set_wrap(group_, false);

  lv_obj_set_size(root_, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t* above_fold_container = lv_obj_create(root_);
  lv_obj_set_layout(above_fold_container, LV_LAYOUT_FLEX);
  lv_obj_set_size(above_fold_container, lv_pct(100), lv_disp_get_ver_res(NULL));
  lv_obj_set_flex_flow(above_fold_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(above_fold_container, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  widgets::TopBar top_bar(above_fold_container, group_);
  top_bar.set_title("Now Playing");

  lv_obj_t* info_container = lv_obj_create(above_fold_container);
  lv_obj_set_layout(info_container, LV_LAYOUT_FLEX);
  lv_obj_set_width(info_container, lv_pct(100));
  lv_obj_set_flex_grow(info_container, 1);
  lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  artist_label_ = info_label(info_container);
  album_label_ = info_label(info_container);
  title_label_ = info_label(info_container);

  scrubber_ = lv_slider_create(above_fold_container);
  lv_obj_set_size(scrubber_, lv_pct(100), 5);
  lv_slider_set_range(scrubber_, 0, 100);
  lv_slider_set_value(scrubber_, 0, LV_ANIM_OFF);

  lv_style_init(&scrubber_style);
  lv_style_set_bg_color(&scrubber_style, lv_color_black());
  lv_obj_add_style(scrubber_, &scrubber_style, LV_PART_INDICATOR);

  lv_group_add_obj(group_, scrubber_);

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

  next_up_header_ = lv_obj_create(above_fold_container);
  lv_obj_set_size(next_up_header_, lv_pct(100), 15);
  lv_obj_set_flex_flow(next_up_header_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(next_up_header_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_END);

  next_up_label_ = lv_label_create(next_up_header_);
  lv_label_set_text(next_up_label_, "Next up...");
  lv_obj_set_height(next_up_label_, lv_pct(100));
  lv_obj_set_flex_grow(next_up_label_, 1);

  lv_obj_t* next_up_hint = lv_label_create(next_up_header_);
  lv_label_set_text(next_up_hint, LV_SYMBOL_DOWN);
  lv_obj_set_size(next_up_hint, LV_SIZE_CONTENT, lv_pct(100));

  next_up_container_ = lv_list_create(root_);
  lv_obj_set_layout(next_up_container_, LV_LAYOUT_FLEX);
  lv_obj_set_size(next_up_container_, lv_pct(100), lv_disp_get_ver_res(NULL));
  lv_obj_set_flex_flow(next_up_container_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(next_up_container_, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  OnTrackUpdate();
  OnQueueUpdate();
}

Playing::~Playing() {}

auto Playing::OnTrackUpdate() -> void {
  auto current = queue_->GetCurrent();
  if (!current) {
    return;
  }
  if (track_ && track_->data().id() == *current) {
    return;
  }
  auto db = db_.lock();
  if (!db) {
    return;
  }
  new_track_.reset(new database::FutureFetcher<std::optional<database::Track>>(
      db->GetTrack(*current)));
}

auto Playing::OnPlaybackUpdate(uint32_t pos_seconds, uint32_t new_duration)
    -> void {
  if (!track_) {
    return;
  }
  lv_slider_set_range(scrubber_, 0, new_duration);
  lv_slider_set_value(scrubber_, pos_seconds, LV_ANIM_ON);
}

auto Playing::OnQueueUpdate() -> void {
  auto current = queue_->GetUpcoming(kMaxUpcoming);
  auto db = db_.lock();
  if (!db) {
    return;
  }
  new_next_tracks_.reset(
      new database::FutureFetcher<std::vector<std::optional<database::Track>>>(
          db->GetBulkTracks(current)));
}

auto Playing::Tick() -> void {
  if (new_track_ && new_track_->Finished()) {
    auto res = new_track_->Result();
    new_track_.reset();
    if (res && *res) {
      BindTrack(**res);
    }
  }
  if (new_next_tracks_ && new_next_tracks_->Finished()) {
    auto res = new_next_tracks_->Result();
    new_next_tracks_.reset();
    if (res) {
      std::vector<database::Track> filtered;
      for (const auto& t : *res) {
        if (t) {
          filtered.push_back(*t);
        }
      }
      ApplyNextUp(filtered);
    }
  }
}

auto Playing::BindTrack(const database::Track& t) -> void {
  track_ = t;

  lv_label_set_text(artist_label_,
                    t.tags().at(database::Tag::kArtist).value_or("").c_str());
  lv_label_set_text(album_label_,
                    t.tags().at(database::Tag::kAlbum).value_or("").c_str());
  lv_label_set_text(title_label_, t.TitleOrFilename().c_str());

  std::optional<int> duration = t.tags().duration;
  lv_slider_set_range(scrubber_, 0, duration.value_or(1));
  lv_slider_set_value(scrubber_, 0, LV_ANIM_OFF);
}

auto Playing::ApplyNextUp(const std::vector<database::Track>& tracks) -> void {
  // TODO(jacqueline): Do a proper diff to maintain selection.
  int children = lv_obj_get_child_cnt(next_up_container_);
  while (children > 0) {
    lv_obj_del(lv_obj_get_child(next_up_container_, 0));
    children--;
  }

  next_tracks_ = tracks;

  if (next_tracks_.empty()) {
    lv_label_set_text(next_up_label_, "Nothing queued");
    return;
  } else {
    lv_label_set_text(next_up_label_, "Next up");
  }

  for (const auto& track : next_tracks_) {
    lv_group_add_obj(
        group_, next_up_label(next_up_container_, track.TitleOrFilename()));
  }
}

auto Playing::OnFocusAboveFold() -> void {
  lv_obj_scroll_to_y(root_, 0, LV_ANIM_ON);
}

auto Playing::OnFocusBelowFold() -> void {
  if (lv_obj_get_scroll_y(root_) < lv_obj_get_y(next_up_header_)) {
    lv_obj_scroll_to_y(root_, lv_obj_get_y(next_up_header_), LV_ANIM_ON);
  }
}

}  // namespace screens
}  // namespace ui
