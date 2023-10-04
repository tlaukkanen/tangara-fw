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

Progress::Progress(Screen* host,
                   std::pmr::string title_text,
                   std::pmr::string subtitle_text)
    : Modal(host) {
  lv_obj_set_layout(root_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  title_ = lv_label_create(root_);
  lv_obj_set_size(title_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  subtitle_ = lv_label_create(root_);
  lv_obj_set_size(subtitle_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  lv_obj_t* spinner = lv_spinner_create(root_, 3000, 45);
  lv_obj_set_size(spinner, 16, 16);

  title(title_text);
  subtitle(subtitle_text);
}

void Progress::title(const std::pmr::string& s) {
  lv_label_set_text(title_, s.c_str());
}

void Progress::subtitle(const std::pmr::string& s) {
  if (s.empty()) {
    lv_obj_add_flag(subtitle_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(subtitle_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(subtitle_, s.c_str());
  }
}

}  // namespace modals
}  // namespace ui
