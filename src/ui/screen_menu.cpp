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
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "misc/lv_area.h"
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

static void item_click_cb(lv_event_t* ev) {
  ESP_LOGI("menu", "clicked!");
}

Menu::Menu() {
  lv_obj_t* list = lv_list_create(root_);
  lv_obj_set_size(list, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_center(list);

  lv_obj_t* button;

  button = lv_list_add_btn(list, NULL, "hi");
  lv_obj_add_event_cb(button, item_click_cb, LV_EVENT_CLICKED, NULL);

  button = lv_list_add_btn(list, NULL, "second");
  lv_obj_add_event_cb(button, item_click_cb, LV_EVENT_CLICKED, NULL);

  button = lv_list_add_btn(list, NULL, "third");
  lv_obj_add_event_cb(button, item_click_cb, LV_EVENT_CLICKED, NULL);
}

Menu::~Menu() {}

}  // namespace screens
}  // namespace ui
