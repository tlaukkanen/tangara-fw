/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "modal_add_to_queue.hpp"

#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "esp_log.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/layouts/flex/lv_flex.h"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "extra/widgets/tabview/lv_tabview.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "source.hpp"
#include "themes.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_btn.h"
#include "widgets/lv_label.h"

namespace ui {
namespace modals {

AddToQueue::AddToQueue(Screen* host,
                       audio::TrackQueue& queue,
                       std::shared_ptr<playlist::IResetableSource> item,
                       bool all_tracks_only)
    : Modal(host), queue_(queue), item_(item), all_tracks_(0) {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER);

  if (all_tracks_only) {
    all_tracks_ = true;
  } else {
    lv_obj_t* button_container = lv_obj_create(root_);
    lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    selected_track_btn_ = lv_btn_create(button_container);
    lv_obj_t* label = lv_label_create(selected_track_btn_);
    lv_label_set_text(label, "Selected");
    lv_group_add_obj(group_, selected_track_btn_);
    lv_obj_add_state(selected_track_btn_, LV_STATE_CHECKED);
    themes::Theme::instance()->ApplyStyle(selected_track_btn_,
                                          themes::Style::kTab);

    lv_bind(selected_track_btn_, LV_EVENT_CLICKED, [this](lv_obj_t*) {
      lv_obj_add_state(selected_track_btn_, LV_STATE_CHECKED);
      lv_obj_clear_state(all_tracks_btn_, LV_STATE_CHECKED);
      all_tracks_ = false;
    });

    all_tracks_btn_ = lv_btn_create(button_container);
    label = lv_label_create(all_tracks_btn_);
    lv_label_set_text(label, "From here");
    lv_group_add_obj(group_, all_tracks_btn_);
    themes::Theme::instance()->ApplyStyle(all_tracks_btn_, themes::Style::kTab);

    lv_bind(all_tracks_btn_, LV_EVENT_CLICKED, [this](lv_obj_t*) {
      lv_obj_clear_state(selected_track_btn_, LV_STATE_CHECKED);
      lv_obj_add_state(all_tracks_btn_, LV_STATE_CHECKED);
      all_tracks_ = true;
    });

    lv_obj_t* spacer = lv_obj_create(root_);
    lv_obj_set_size(spacer, 1, 4);
  }

  lv_obj_t* button_container = lv_obj_create(root_);
  lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* btn = lv_btn_create(button_container);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, "Play now");
  lv_group_add_obj(group_, btn);

  lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
    queue_.Clear();
    if (all_tracks_) {
      queue_.IncludeNext(item_);
    } else {
      auto track = item_->Current();
      if (track) {
        queue_.AddNext(*track);
      }
    }
    events::Ui().Dispatch(internal::ModalCancelPressed{});
    events::Ui().Dispatch(internal::ShowNowPlaying{});
  });

  bool has_queue = queue.GetCurrent().has_value();

  if (has_queue) {
    label = lv_label_create(root_);
    lv_label_set_text(label, "Enqueue");

    lv_obj_t* spacer = lv_obj_create(root_);
    lv_obj_set_size(spacer, 1, 4);

    button_container = lv_obj_create(root_);
    lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btn = lv_btn_create(button_container);
    label = lv_label_create(btn);
    lv_label_set_text(label, "Next");
    lv_group_add_obj(group_, btn);

    lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
      if (all_tracks_) {
        queue_.IncludeNext(item_);
      } else {
        queue_.AddNext(item_->Current().value());
      }
      events::Ui().Dispatch(internal::ModalCancelPressed{});
    });

    btn = lv_btn_create(button_container);
    label = lv_label_create(btn);
    lv_label_set_text(label, "Last");
    lv_group_add_obj(group_, btn);

    lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
      if (all_tracks_) {
        queue_.IncludeLast(item_);
      } else {
        queue_.AddLast(item_->Current().value());
      }
      events::Ui().Dispatch(internal::ModalCancelPressed{});
    });
  }

  lv_obj_t* spacer = lv_obj_create(root_);
  lv_obj_set_size(spacer, 1, 4);

  button_container = lv_obj_create(root_);
  lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  btn = lv_btn_create(button_container);
  label = lv_label_create(btn);
  lv_label_set_text(label, "Cancel");
  lv_group_add_obj(group_, btn);
  lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED),
                              LV_PART_MAIN);

  lv_bind(btn, LV_EVENT_CLICKED, [](lv_obj_t*) {
    events::Ui().Dispatch(internal::ModalCancelPressed{});
  });
}

}  // namespace modals
}  // namespace ui
