/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <vector>

#include "database.hpp"
#include "index.hpp"
#include "lvgl.h"

#include "modal.hpp"
#include "source.hpp"
#include "track_queue.hpp"

namespace ui {
namespace modals {

class AddToQueue : public Modal {
 public:
  AddToQueue(Screen*,
             audio::TrackQueue&,
             std::shared_ptr<playlist::IResetableSource>,
             bool all_tracks_only = false);

 private:
  audio::TrackQueue& queue_;
  std::shared_ptr<playlist::IResetableSource> item_;
  lv_obj_t* container_;

  lv_obj_t* selected_track_btn_;
  lv_obj_t* all_tracks_btn_;
  bool all_tracks_;
};

}  // namespace modals
}  // namespace ui
