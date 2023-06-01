/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "lvgl.h"

namespace ui {

class Screen {
 public:
  Screen() : root_(lv_obj_create(NULL)) {}
  virtual ~Screen() { lv_obj_del(root_); }

  auto root() -> lv_obj_t* { return root_; }

 protected:
  lv_obj_t* const root_;
};

}  // namespace ui
