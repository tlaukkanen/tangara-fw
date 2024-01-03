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
#include "model_top_bar.hpp"
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

  // Disable wrapping by default, since it's confusing and generally makes it
  // harder to navigate quickly.
  lv_group_set_wrap(group_, false);
}

Screen::~Screen() {
  // The group *must* be deleted first. Otherwise, focus events will be
  // generated whilst deleting the object tree, which causes a big mess.
  lv_group_del(group_);
  lv_obj_del(root_);
}

auto Screen::CreateTopBar(lv_obj_t* parent,
                          const widgets::TopBar::Configuration& config,
                          models::TopBar& model) -> widgets::TopBar* {
  assert(top_bar_ == nullptr);
  top_bar_ = std::make_unique<widgets::TopBar>(parent, config, model);
  if (top_bar_->button()) {
    lv_group_add_obj(group_, top_bar_->button());
  }
  return top_bar_.get();
}

MenuScreen::MenuScreen(models::TopBar& top_bar_model,
                       const std::pmr::string& title,
                       bool show_back_button)
    : Screen() {
  lv_group_set_wrap(group_, false);

  lv_obj_set_layout(content_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  widgets::TopBar::Configuration config{
      .show_back_button = show_back_button,
      .title = title.c_str(),
  };
  CreateTopBar(content_, config, top_bar_model);

  content_ = lv_obj_create(content_);
  lv_obj_set_flex_grow(content_, 1);
  lv_obj_set_width(content_, lv_pct(100));
  lv_obj_set_layout(content_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  lv_obj_set_style_pad_all(content_, 4, LV_PART_MAIN);
}

}  // namespace ui
