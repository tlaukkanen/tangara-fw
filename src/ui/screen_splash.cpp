/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_splash.hpp"

#include "lvgl.h"

LV_IMG_DECLARE(splash);

namespace ui {
namespace screens {

Splash::Splash() {
  lv_obj_t *logo = lv_img_create(root_);
  lv_img_set_src(logo, &splash);
  lv_obj_center(logo);
}

Splash::~Splash() {}

}  // namespace screens
}  // namespace ui
