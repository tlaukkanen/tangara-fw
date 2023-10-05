/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_playing.hpp"
#include <sys/_stdint.h>
#include <memory>

#include "audio_events.hpp"
#include "bindey/binding.h"
#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "core/lv_obj_scroll.h"
#include "core/lv_obj_tree.h"
#include "database.hpp"
#include "esp_log.h"
#include "extra/layouts/flex/lv_flex.h"
#include "extra/layouts/grid/lv_grid.h"
#include "font/lv_symbol_def.h"
#include "font_symbols.hpp"
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
#include "model_playback.hpp"
#include "model_top_bar.hpp"
#include "track.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_btn.h"
#include "widgets/lv_img.h"
#include "widgets/lv_label.h"
#include "widgets/lv_slider.h"

namespace ui {
namespace screens {

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
  lv_label_set_text(label, "");
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

auto Playing::next_up_label(lv_obj_t* parent, const std::pmr::string& text)
    -> lv_obj_t* {
  lv_obj_t* button = lv_list_add_btn(parent, NULL, text.c_str());
  lv_label_set_long_mode(lv_obj_get_child(button, -1), LV_LABEL_LONG_DOT);
  lv_obj_add_event_cb(button, below_fold_focus_cb, LV_EVENT_FOCUSED, this);
  lv_group_add_obj(group_, button);
  return button;
}

Playing::Playing(models::TopBar& top_bar_model,
                 models::Playback& playback_model,
                 std::weak_ptr<database::Database> db,
                 audio::TrackQueue& queue)
    : db_(db),
      queue_(queue),
      current_track_(),
      next_tracks_(),
      new_track_(),
      new_next_tracks_() {
  lv_obj_set_layout(content_, LV_LAYOUT_FLEX);
  lv_group_set_wrap(group_, false);

  lv_obj_set_size(content_, lv_pct(100), lv_disp_get_ver_res(NULL));
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t* above_fold_container = lv_obj_create(content_);
  lv_obj_set_layout(above_fold_container, LV_LAYOUT_FLEX);
  lv_obj_set_size(above_fold_container, lv_pct(100), lv_disp_get_ver_res(NULL));
  lv_obj_set_flex_flow(above_fold_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(above_fold_container, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  widgets::TopBar::Configuration config{
      .show_back_button = true,
      .title = "Now Playing",
  };
  CreateTopBar(above_fold_container, config, top_bar_model);

  lv_obj_t* now_playing_container = lv_obj_create(above_fold_container);
  lv_obj_set_layout(now_playing_container, LV_LAYOUT_FLEX);
  lv_obj_set_width(now_playing_container, lv_pct(100));
  lv_obj_set_flex_grow(now_playing_container, 1);
  lv_obj_set_flex_flow(now_playing_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(now_playing_container, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_set_style_pad_left(now_playing_container, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_right(now_playing_container, 4, LV_PART_MAIN);

  lv_obj_t* info_container = lv_obj_create(now_playing_container);
  lv_obj_set_layout(info_container, LV_LAYOUT_FLEX);
  lv_obj_set_width(info_container, lv_pct(100));
  lv_obj_set_flex_grow(info_container, 1);
  lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* artist_label = info_label(info_container);
  lv_obj_t* album_label = info_label(info_container);
  lv_obj_t* title_label = info_label(info_container);

  lv_obj_t* scrubber = lv_slider_create(now_playing_container);
  lv_obj_set_size(scrubber, lv_pct(100), 5);

  lv_style_init(&scrubber_style);
  lv_style_set_bg_color(&scrubber_style, lv_color_black());
  lv_obj_add_style(scrubber, &scrubber_style, LV_PART_INDICATOR);

  lv_group_add_obj(group_, scrubber);

  data_bindings_.emplace_back(playback_model.current_track.onChangedAndNow(
      [=, this](const std::optional<database::TrackId>& id) {
        if (!id) {
          return;
        }
        if (current_track_.get() && current_track_.get()->data().id() == *id) {
          return;
        }
        auto db = db_.lock();
        if (!db) {
          return;
        }
        // Clear the playback progress whilst we're waiting for the next
        // track's data to load.
        lv_slider_set_value(scrubber, 0, LV_ANIM_OFF);
        new_track_.reset(
            new database::FutureFetcher<std::shared_ptr<database::Track>>(
                db->GetTrack(*id)));
      }));

  data_bindings_.emplace_back(current_track_.onChangedAndNow(
      [=](const std::shared_ptr<database::Track>& t) {
        if (!t) {
          return;
        }
        lv_label_set_text(
            artist_label,
            t->tags().at(database::Tag::kArtist).value_or("").c_str());
        lv_label_set_text(
            album_label,
            t->tags().at(database::Tag::kAlbum).value_or("").c_str());
        lv_label_set_text(title_label, t->TitleOrFilename().c_str());
      }));

  data_bindings_.emplace_back(
      playback_model.current_track_duration.onChangedAndNow([=](uint32_t d) {
        lv_slider_set_range(scrubber, 0, std::max<uint32_t>(1, d));
      }));
  data_bindings_.emplace_back(
      playback_model.current_track_position.onChangedAndNow(
          [=](uint32_t p) { lv_slider_set_value(scrubber, p, LV_ANIM_OFF); }));

  lv_obj_t* spacer = lv_obj_create(now_playing_container);
  lv_obj_set_size(spacer, 1, 4);

  lv_obj_t* controls_container = lv_obj_create(now_playing_container);
  lv_obj_set_size(controls_container, lv_pct(100), 20);
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* play_pause_control =
      control_button(controls_container, LV_SYMBOL_PLAY);
  lv_group_add_obj(group_, play_pause_control);
  lv_bind(play_pause_control, LV_EVENT_CLICKED, [=](lv_obj_t*) {
    events::Audio().Dispatch(audio::TogglePlayPause{});
  });

  lv_obj_t* track_prev = control_button(controls_container, LV_SYMBOL_PREV);
  lv_group_add_obj(group_, track_prev);
  lv_bind(track_prev, LV_EVENT_CLICKED, [=](lv_obj_t*) { queue_.Previous(); });

  lv_obj_t* track_next = control_button(controls_container, LV_SYMBOL_NEXT);
  lv_group_add_obj(group_, track_next);
  lv_bind(track_next, LV_EVENT_CLICKED, [=](lv_obj_t*) { queue_.Next(); });

  lv_obj_t* shuffle = control_button(controls_container, LV_SYMBOL_SHUFFLE);
  lv_group_add_obj(group_, shuffle);
  // lv_bind(shuffle, LV_EVENT_CLICKED, [=](lv_obj_t*) { queue_ });

  lv_group_add_obj(group_, control_button(controls_container, LV_SYMBOL_LOOP));

  next_up_header_ = lv_obj_create(now_playing_container);
  lv_obj_set_size(next_up_header_, lv_pct(100), 15);
  lv_obj_set_flex_flow(next_up_header_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(next_up_header_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_END);

  next_up_label_ = lv_label_create(next_up_header_);
  lv_label_set_text(next_up_label_, "");
  lv_obj_set_height(next_up_label_, lv_pct(100));
  lv_obj_set_flex_grow(next_up_label_, 1);

  next_up_hint_ = lv_label_create(next_up_header_);
  lv_label_set_text(next_up_hint_, "");
  lv_obj_set_style_text_font(next_up_hint_, &font_symbols, 0);
  lv_obj_set_size(next_up_hint_, LV_SIZE_CONTENT, lv_pct(100));

  lv_obj_t* next_up_spinner_ = lv_spinner_create(next_up_header_, 4000, 30);
  lv_obj_set_size(next_up_spinner_, 12, 12);

  next_up_container_ = lv_list_create(content_);
  lv_obj_set_layout(next_up_container_, LV_LAYOUT_FLEX);
  lv_obj_set_size(next_up_container_, lv_pct(100), lv_disp_get_ver_res(NULL));
  lv_obj_set_flex_flow(next_up_container_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(next_up_container_, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  data_bindings_.emplace_back(playback_model.upcoming_tracks.onChangedAndNow(
      [=, this](const std::vector<database::TrackId>& ids) {
        auto db = db_.lock();
        if (!db) {
          return;
        }
        lv_label_set_text(next_up_label_, "Next up");
        lv_obj_add_flag(next_up_hint_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(next_up_spinner_, LV_OBJ_FLAG_HIDDEN);

        new_next_tracks_.reset(new database::FutureFetcher<
                               std::vector<std::shared_ptr<database::Track>>>(
            db->GetBulkTracks(ids)));
      }));

  data_bindings_.emplace_back(next_tracks_.onChangedAndNow(
      [=](const std::vector<std::shared_ptr<database::Track>>& tracks) {
        lv_obj_clear_flag(next_up_hint_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(next_up_spinner_, LV_OBJ_FLAG_HIDDEN);

        // TODO(jacqueline): Do a proper diff to maintain selection.
        int children = lv_obj_get_child_cnt(next_up_container_);
        while (children > 0) {
          lv_obj_del(lv_obj_get_child(next_up_container_, 0));
          children--;
        }

        if (tracks.empty()) {
          lv_label_set_text(next_up_label_, "Nothing queued");
          lv_label_set_text(next_up_hint_, "");
          return;
        } else {
          lv_label_set_text(next_up_label_, "Next up");
          lv_label_set_text(next_up_hint_, "");
        }

        for (const auto& track : tracks) {
          lv_group_add_obj(group_, next_up_label(next_up_container_,
                                                 track->TitleOrFilename()));
        }
      }));
}

Playing::~Playing() {}

auto Playing::Tick() -> void {
  if (new_track_ && new_track_->Finished()) {
    auto res = new_track_->Result();
    new_track_.reset();
    if (res) {
      current_track_(*res);
    }
  }
  if (new_next_tracks_ && new_next_tracks_->Finished()) {
    auto res = new_next_tracks_->Result();
    new_next_tracks_.reset();
    if (res) {
      std::vector<std::shared_ptr<database::Track>> filtered;
      for (const auto& t : *res) {
        if (t) {
          filtered.push_back(t);
        }
      }
      next_tracks_.set(filtered);
    }
  }
}

auto Playing::OnFocusAboveFold() -> void {
  lv_obj_scroll_to_y(content_, 0, LV_ANIM_ON);
}

auto Playing::OnFocusBelowFold() -> void {
  if (lv_obj_get_scroll_y(content_) < lv_obj_get_y(next_up_header_) + 20) {
    lv_obj_scroll_to_y(content_, lv_obj_get_y(next_up_header_) + 20,
                       LV_ANIM_ON);
  }
}

}  // namespace screens
}  // namespace ui
