/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen.hpp"

#include <memory>

#include "core/lv_obj_pos.h"
#include "core/lv_obj_tree.h"
#include "hal/lv_hal_disp.h"
#include "misc/lv_area.h"
#include "misc/lv_color.h"

namespace ui {

Screen::Screen()
    : root_(lv_obj_create(NULL)),
      content_(lv_obj_create(root_)),
      modal_content_(lv_obj_create(root_)),
      alert_(lv_obj_create(root_)),
      group_(lv_group_create()),
      modal_group_(nullptr) {
  lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
  lv_obj_set_size(content_, lv_pct(100), lv_pct(100));
  lv_obj_set_size(modal_content_, lv_pct(100), lv_pct(100));
  lv_obj_set_size(alert_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_center(root_);
  lv_obj_center(content_);
  lv_obj_center(modal_content_);
  lv_obj_center(alert_);

  lv_obj_set_style_bg_opa(modal_content_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(alert_, LV_OPA_TRANSP, 0);

  lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);

  // Disable wrapping by default, since it's confusing and generally makes it
  // harder to navigate quickly.
  lv_group_set_wrap(group_, false);
}

Screen::~Screen() {
  // The group *must* be deleted first. Otherwise, focus events will be
  // generated whilst deleting the object tree, which causes a big mess.
  lv_group_del(group_);
  lv_obj_del(root_);
}

}  // namespace ui
