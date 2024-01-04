/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "bindey/binding.h"
#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "event_binding.hpp"
#include "lvgl.h"
#include "model_top_bar.hpp"
#include "nod/nod.hpp"
#include "widget_top_bar.hpp"

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

  /*
   * Called periodically to allow the screen to update itself, e.g. to handle
   * std::futures that are still loading in.
   */
  virtual auto Tick() -> void {}

  auto root() -> lv_obj_t* { return root_; }
  auto content() -> lv_obj_t* { return content_; }
  auto alert() -> lv_obj_t* { return alert_; }

  auto modal_content() -> lv_obj_t* { return modal_content_; }
  auto modal_group(lv_group_t* g) -> void { modal_group_ = g; }
  auto group() -> lv_group_t* {
    if (modal_group_) {
      return modal_group_;
    }
    return group_;
  }

 protected:
  auto CreateTopBar(lv_obj_t* parent,
                    const widgets::TopBar::Configuration&,
                    models::TopBar& model) -> widgets::TopBar*;

  std::pmr::vector<bindey::scoped_binding> data_bindings_;
  std::pmr::vector<std::unique_ptr<EventBinding>> event_bindings_;

  template <typename T>
  auto lv_bind(lv_obj_t* obj, lv_event_code_t ev, T fn) -> void {
    auto binding = std::make_unique<EventBinding>(obj, ev);
    binding->signal().connect(fn);
    event_bindings_.push_back(std::move(binding));
  }

  lv_obj_t* const root_;
  lv_obj_t* content_;
  lv_obj_t* modal_content_;
  lv_obj_t* alert_;

  lv_group_t* const group_;
  lv_group_t* modal_group_;

 private:
  std::unique_ptr<widgets::TopBar> top_bar_;
};

class MenuScreen : public Screen {
 public:
  MenuScreen(models::TopBar&,
             const std::pmr::string& title,
             bool show_back_button = true);
};

}  // namespace ui
