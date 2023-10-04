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

#include "modal.hpp"

namespace ui {
namespace modals {

class Progress : public Modal {
 public:
  Progress(Screen*, std::pmr::string title, std::pmr::string subtitle = "");

  void title(const std::pmr::string&);
  void subtitle(const std::pmr::string&);

 private:
  lv_obj_t* container_;
  lv_obj_t* title_;
  lv_obj_t* subtitle_;
};

}  // namespace modals
}  // namespace ui
