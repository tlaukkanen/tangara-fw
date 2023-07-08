/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_menu.hpp"

#include "core/lv_event.h"
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
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

static void item_click_cb(lv_event_t* ev) {
  if (ev->user_data == NULL) {
    return;
  }
  database::IndexInfo* index =
      reinterpret_cast<database::IndexInfo*>(ev->user_data);

  events::Dispatch<internal::IndexSelected, UiState>(
      internal::IndexSelected{.index = *index});
}

Menu::Menu(std::vector<database::IndexInfo> indexes) : indexes_(indexes) {
  lv_obj_t* list = lv_list_create(root_);
  lv_obj_set_size(list, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_center(list);

  for (database::IndexInfo& index : indexes_) {
    lv_obj_t* item = lv_list_add_btn(list, NULL, index.name.c_str());
    lv_obj_set_style_bg_color(item, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, &index);
    lv_group_add_obj(group_, item);
  }
}

Menu::~Menu() {}

}  // namespace screens
}  // namespace ui
