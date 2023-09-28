/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <string>

#include "bindey/binding.h"
#include "lvgl.h"

#include "memory_resource.hpp"
#include "model_top_bar.hpp"

namespace ui {

namespace widgets {

class TopBar {
 public:
  struct Configuration {
    bool show_back_button;
    std::pmr::string title;
  };

  explicit TopBar(lv_obj_t* parent,
                  const Configuration& config,
                  models::TopBar& model);

  auto root() -> lv_obj_t* { return container_; }
  auto button() -> lv_obj_t* { return back_button_; }

 private:
  std::vector<bindey::scoped_binding> bindings_;

  lv_obj_t* container_;
  lv_obj_t* back_button_;
};

}  // namespace widgets

}  // namespace ui
