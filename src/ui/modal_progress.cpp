/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "modal_progress.hpp"

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
#include "widgets/lv_label.h"

namespace ui {
namespace modals {

Progress::Progress(Screen* host, std::string title_text) : Modal(host) {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* title = lv_label_create(root_);
  lv_label_set_text(title, title_text.c_str());
  lv_obj_set_size(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  lv_obj_t* spinner = lv_spinner_create(root_, 3000, 45);
  lv_obj_set_size(spinner, 16, 16);
}

}  // namespace modals
}  // namespace ui
