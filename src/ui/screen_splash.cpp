/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_splash.hpp"

#include "core/lv_obj_pos.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "misc/lv_area.h"
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

Splash::Splash() {
  container_ = lv_obj_create(root_);
  lv_obj_set_align(container_, LV_ALIGN_CENTER);
  lv_obj_set_size(container_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  label_ = lv_label_create(container_);
  lv_label_set_text_static(label_, "TANGARA");
  lv_obj_set_align(label_, LV_ALIGN_TOP_MID);

  spinner_ = lv_spinner_create(container_, 1000, 60);
  lv_obj_set_size(spinner_, 32, 32);
  lv_obj_align_to(spinner_, label_, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
}

Splash::~Splash() {
  lv_obj_del(spinner_);
  lv_obj_del(label_);
  lv_obj_del(container_);
}

}  // namespace screens
}  // namespace ui
