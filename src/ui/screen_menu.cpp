/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_menu.hpp"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "misc/lv_area.h"
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

Menu::Menu() {
  lv_obj_t* menu = lv_menu_create(root_);
  lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_center(menu);

  lv_obj_t* main_page = lv_menu_page_create(menu, NULL);

  lv_obj_t* container;
  lv_obj_t* label;

  container = lv_menu_cont_create(main_page);
  label = lv_label_create(container);
  lv_label_set_text(label, "I am an item");

  container = lv_menu_cont_create(main_page);
  label = lv_label_create(container);
  lv_label_set_text(label, "I am also an item");

  container = lv_menu_cont_create(main_page);
  label = lv_label_create(container);
  lv_label_set_text(label, "Item #3");

  container = lv_menu_cont_create(main_page);
  label = lv_label_create(container);
  lv_label_set_text(label, "Yay!");

  lv_menu_set_page(menu, main_page);
}

Menu::~Menu() {}

}  // namespace screens
}  // namespace ui
