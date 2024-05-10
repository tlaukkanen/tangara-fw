/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "lvgl.h"

#include "ui/screen.hpp"

namespace ui {

class Modal {
 public:
  Modal(Screen* host);
  virtual ~Modal();

  auto root() -> lv_obj_t* { return root_; }
  auto group() -> lv_group_t* { return group_; }

 protected:
  lv_obj_t* const root_;
  lv_group_t* const group_;

 private:
  Screen* host_;
};

}  // namespace ui
