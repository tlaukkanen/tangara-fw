/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "lvgl.h"

namespace ui {

namespace widgets {

class TopBar {
 public:
  TopBar(lv_obj_t* parent, lv_group_t* group);

  auto set_title(const std::string&) -> void;

  lv_obj_t* container_;
  lv_obj_t* title_;
  lv_obj_t* back_button_;
};

}  // namespace widgets

}  // namespace ui
