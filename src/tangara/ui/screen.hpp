/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "lvgl.h"

namespace ui {

/*
 * Base class for ever discrete screen in the app. Provides a consistent
 * interface that can be used for transitioning between screens, adding them to
 * back stacks, etc.
 */
class Screen {
 public:
  Screen();
  virtual ~Screen();

  virtual auto onShown() -> void {}
  virtual auto onHidden() -> void {}

  auto root() -> lv_obj_t* { return root_; }
  auto content() -> lv_obj_t* { return content_; }
  auto alert() -> lv_obj_t* { return alert_; }

  auto group() -> lv_group_t* { return group_; }

  virtual auto canPop() -> bool = 0;

 protected:
  lv_obj_t* const root_;
  lv_obj_t* content_;
  lv_obj_t* alert_;

  lv_group_t* const group_;
};

}  // namespace ui
