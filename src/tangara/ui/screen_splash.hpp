/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "lvgl.h"

#include "ui/screen.hpp"

namespace ui {
namespace screens {

class Splash : public Screen {
 public:
  Splash();
  ~Splash();

  auto canPop() -> bool override { return true; }

 private:
  lv_obj_t* container_;
  lv_obj_t* label_;
  lv_obj_t* spinner_;
};

}  // namespace screens
}  // namespace ui
