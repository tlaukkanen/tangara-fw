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

namespace ui {

/*
 * Base class for ever discrete screen in the app. Provides a consistent
 * interface that can be used for transitioning between screens, adding them to
 * back stacks, etc.
 */
class Screen {
 public:
  Screen() : root_(lv_obj_create(NULL)), group_(lv_group_create()) {}

  virtual ~Screen() {
    // The group *must* be deleted first. Otherwise, focus events will be
    // generated whilst deleting the object tree, which causes a big mess.
    lv_group_del(group_);
    lv_obj_del(root_);
  }

  /*
   * Called periodically to allow the screen to update itself, e.g. to handle
   * std::futures that are still loading in.
   */
  virtual auto Tick() -> void {}

  auto UpdateTopBar(const widgets::TopBar::State& state) -> void;

  auto root() -> lv_obj_t* { return root_; }
  auto group() -> lv_group_t* { return group_; }

 protected:
  auto CreateTopBar(lv_obj_t* parent, const widgets::TopBar::Configuration&)
      -> widgets::TopBar*;

  lv_obj_t* const root_;
  lv_group_t* const group_;

 private:
  std::unique_ptr<widgets::TopBar> top_bar_;
};

}  // namespace ui
