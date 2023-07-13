/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen.hpp"

#include <memory>

#include "widget_top_bar.hpp"

namespace ui {

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
