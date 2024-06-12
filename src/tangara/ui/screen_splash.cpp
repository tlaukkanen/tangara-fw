/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui/screen_splash.hpp"

#include "core/lv_obj.h"
#include "core/lv_obj_style.h"
#include "lvgl.h"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"

LV_IMG_DECLARE(splash);

namespace ui {
namespace screens {

Splash::Splash() {
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);

  lv_obj_t* logo = lv_img_create(root_);
  lv_obj_set_style_bg_opa(logo, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(logo, lv_color_black(), 0);
  lv_img_set_src(logo, &splash);
  lv_obj_center(logo);
}

Splash::~Splash() {}

}  // namespace screens
}  // namespace ui
