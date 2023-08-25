/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen.hpp"

#include <memory>

#include "core/lv_obj_pos.h"
#include "core/lv_obj_tree.h"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "widget_top_bar.hpp"

namespace ui {

Screen::Screen()
    : root_(lv_obj_create(NULL)),
      content_(lv_obj_create(root_)),
      modal_content_(lv_obj_create(root_)),
      group_(lv_group_create()),
      modal_group_(nullptr) {
  lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
  lv_obj_set_size(content_, lv_pct(100), lv_pct(100));
  lv_obj_set_size(modal_content_, lv_pct(100), lv_pct(100));
  lv_obj_center(root_);
  lv_obj_center(content_);
  lv_obj_center(modal_content_);

  lv_obj_set_style_bg_opa(modal_content_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(modal_content_, lv_color_black(), 0);
}

Screen::~Screen() {
  // The group *must* be deleted first. Otherwise, focus events will be
  // generated whilst deleting the object tree, which causes a big mess.
  lv_group_del(group_);
  lv_obj_del(root_);
}

auto Screen::UpdateTopBar(const widgets::TopBar::State& state) -> void {
  if (top_bar_) {
    top_bar_->Update(state);
  }
}

auto Screen::CreateTopBar(lv_obj_t* parent,
                          const widgets::TopBar::Configuration& config)
    -> widgets::TopBar* {
  assert(top_bar_ == nullptr);
  top_bar_ = std::make_unique<widgets::TopBar>(parent, config);
  if (top_bar_->button()) {
    lv_group_add_obj(group_, top_bar_->button());
  }
  return top_bar_.get();
}

}  // namespace ui
