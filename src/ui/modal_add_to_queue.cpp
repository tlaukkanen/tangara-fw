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
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_area.h"
#include "source.hpp"
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
                       std::shared_ptr<playlist::IndexRecordSource> item)
    : Modal(host), queue_(queue), item_(item) {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* label = lv_label_create(root_);
  lv_label_set_text(label, "This track");

  lv_obj_t* button_container = lv_obj_create(root_);
  lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* btn = lv_btn_create(button_container);
  label = lv_label_create(btn);
  lv_label_set_text(label, "Play");
  lv_group_add_obj(group_, btn);

  lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
    queue_.Clear();
    auto track = item_->Current();
    if (track) {
      queue_.AddNext(*track);
    }
    events::Ui().Dispatch(internal::ModalCancelPressed{});
  });

  bool has_queue = queue.GetCurrent().has_value();

  if (has_queue) {
    btn = lv_btn_create(button_container);
    label = lv_label_create(btn);
    lv_label_set_text(label, "Enqueue");
    lv_group_add_obj(group_, btn);

    lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
      auto track = item_->Current();
      if (track) {
        queue_.AddLast(*track);
      }
      events::Ui().Dispatch(internal::ModalCancelPressed{});
    });
  }
  label = lv_label_create(root_);
  lv_label_set_text(label, "All tracks");

  button_container = lv_obj_create(root_);
  lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  btn = lv_btn_create(button_container);
  label = lv_label_create(btn);
  lv_label_set_text(label, "Play");
  lv_group_add_obj(group_, btn);

  lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
    queue_.Clear();
    queue_.IncludeNext(item_);
    events::Ui().Dispatch(internal::ModalCancelPressed{});
  });

  if (has_queue) {
    btn = lv_btn_create(button_container);
    label = lv_label_create(btn);
    lv_label_set_text(label, "Enqueue");
    lv_group_add_obj(group_, btn);

    lv_bind(btn, LV_EVENT_CLICKED, [this](lv_obj_t*) {
      queue_.IncludeLast(item_);
      events::Ui().Dispatch(internal::ModalCancelPressed{});
    });
  }

  btn = lv_btn_create(root_);
  label = lv_label_create(btn);
  lv_label_set_text(label, "Cancel");
  lv_group_add_obj(group_, btn);

  lv_bind(btn, LV_EVENT_CLICKED, [](lv_obj_t*) {
    events::Ui().Dispatch(internal::ModalCancelPressed{});
  });
}

}  // namespace modals
}  // namespace ui
