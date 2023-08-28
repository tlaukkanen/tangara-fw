/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <string>

#include "lvgl.h"

namespace ui {

namespace widgets {

class TopBar {
 public:
  struct Configuration {
    bool show_back_button;
    std::string title;
  };

  enum class PlaybackState {
    kIdle,
    kPaused,
    kPlaying,
  };

  struct State {
    PlaybackState playback_state;
    uint_fast8_t battery_percent;
    bool is_charging;
  };

  explicit TopBar(lv_obj_t* parent, const Configuration& config);

  auto root() -> lv_obj_t* { return container_; }
  auto button() -> lv_obj_t* { return back_button_; }

  auto Update(const State&) -> void;

 private:
  lv_obj_t* container_;

  lv_obj_t* back_button_;
  lv_obj_t* title_;
  lv_obj_t* playback_;
  lv_obj_t* battery_;
  lv_obj_t* charging_;
};

}  // namespace widgets

}  // namespace ui
