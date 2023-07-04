/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <vector>

#include "index.hpp"
#include "lvgl.h"

#include "screen.hpp"

namespace ui {
namespace screens {

class Menu : public Screen {
 public:
  explicit Menu(std::vector<database::IndexInfo> indexes);
  ~Menu();

 private:
  std::vector<database::IndexInfo> indexes_;
  lv_obj_t* container_;
  lv_obj_t* label_;
};

}  // namespace screens
}  // namespace ui
