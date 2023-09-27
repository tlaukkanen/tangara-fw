/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <sys/_stdint.h>
#include <memory>
#include <vector>

#include "bindey/property.h"
#include "esp_log.h"
#include "lvgl.h"

#include "database.hpp"
#include "future_fetcher.hpp"
#include "model_playback.hpp"
#include "screen.hpp"
#include "track.hpp"
#include "track_queue.hpp"

namespace ui {
namespace screens {

/*
 * The 'Now Playing' / 'Currently Playing' screen that contains information
 * about the current track, as well as playback controls.
 */
class Playing : public Screen {
 public:
  explicit Playing(models::Playback& playback_model,
                   std::weak_ptr<database::Database> db,
                   audio::TrackQueue& queue);
  ~Playing();

  auto Tick() -> void override;

  auto OnFocusAboveFold() -> void;
  auto OnFocusBelowFold() -> void;

  Playing(const Playing&) = delete;
  Playing& operator=(const Playing&) = delete;

 private:
  auto control_button(lv_obj_t* parent, char* icon) -> lv_obj_t*;
  auto next_up_label(lv_obj_t* parent, const std::pmr::string& text)
      -> lv_obj_t*;

  std::weak_ptr<database::Database> db_;
  audio::TrackQueue& queue_;

  bindey::property<std::shared_ptr<database::Track>> current_track_;
  bindey::property<std::vector<std::shared_ptr<database::Track>>> next_tracks_;

  std::unique_ptr<database::FutureFetcher<std::shared_ptr<database::Track>>>
      new_track_;
  std::unique_ptr<
      database::FutureFetcher<std::vector<std::shared_ptr<database::Track>>>>
      new_next_tracks_;

  lv_obj_t* next_up_header_;
  lv_obj_t* next_up_label_;
  lv_obj_t* next_up_hint_;
  lv_obj_t* next_up_container_;
};

}  // namespace screens
}  // namespace ui
