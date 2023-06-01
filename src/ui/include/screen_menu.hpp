/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "lvgl.h"

#include "screen.hpp"

namespace ui {
namespace screens {

class Menu : public Screen {
 public:
  Menu();
  ~Menu();

 private:
  lv_obj_t* container_;
  lv_obj_t* label_;
};

}  // namespace screens
}  // namespace ui
