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
#include "widget_top_bar.hpp"

#include "screen.hpp"

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

  std::pmr::vector<std::unique_ptr<EventBinding>> event_bindings_;

  template <typename T>
  auto lv_bind(lv_obj_t* obj, lv_event_code_t ev, T fn) -> void {
    auto binding = std::make_unique<EventBinding>(obj, ev);
    binding->signal().connect(fn);
    event_bindings_.push_back(std::move(binding));
  }

 private:
  Screen* host_;
};

}  // namespace ui
