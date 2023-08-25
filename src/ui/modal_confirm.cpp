/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "modal_confirm.hpp"

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
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_btn.h"
#include "widgets/lv_label.h"

namespace ui {
namespace modals {

static void button_cancel_cb(lv_event_t* e) {
  events::Ui().Dispatch(internal::ModalCancelPressed{});
}

static void button_confirm_cb(lv_event_t* e) {
  events::Ui().Dispatch(internal::ModalConfirmPressed{});
}

Confirm::Confirm(Screen* host, const std::string& title_text, bool has_cancel)
    : Modal(host) {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* title = lv_label_create(root_);
  lv_label_set_text(title, title_text.c_str());
  lv_obj_set_size(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  lv_obj_t* button_container = lv_obj_create(root_);
  lv_obj_set_size(button_container, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_layout(button_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  if (has_cancel) {
    lv_obj_t* cancel_btn = lv_btn_create(button_container);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_group_add_obj(group_, cancel_btn);
    lv_obj_add_event_cb(cancel_btn, button_cancel_cb, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_t* ok_btn = lv_btn_create(button_container);
  lv_obj_t* ok_label = lv_label_create(ok_btn);
  lv_label_set_text(ok_label, "Okay");
  lv_group_add_obj(group_, ok_btn);
  lv_obj_add_event_cb(ok_btn, button_confirm_cb, LV_EVENT_CLICKED, NULL);
}

}  // namespace modals
}  // namespace ui
