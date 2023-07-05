/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <memory>
#include <vector>

#include "lvgl.h"

#include "database.hpp"
#include "screen.hpp"
#include "track.hpp"

namespace ui {
namespace screens {

class Playing : public Screen {
 public:
  explicit Playing(database::Track t);
  ~Playing();

  auto BindTrack(database::Track t) -> void;

  auto UpdateTime(uint32_t) -> void;
  auto UpdateNextUp(std::vector<database::Track> tracks) -> void;

 private:
  database::Track track_;

  lv_obj_t* artist_label_;
  lv_obj_t* album_label_;
  lv_obj_t* title_label_;

  lv_obj_t* scrubber_;
  lv_obj_t* play_pause_control_;

  lv_obj_t* next_up_container_;
  std::vector<database::Track> next_tracks_;
};

}  // namespace screens
}  // namespace ui
