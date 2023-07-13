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

#include "lvgl.h"

#include "database.hpp"
#include "future_fetcher.hpp"
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
  explicit Playing(std::weak_ptr<database::Database> db,
                   audio::TrackQueue* queue);
  ~Playing();

  auto Tick() -> void override;

  // Callbacks invoked by the UI state machine in response to audio events.

  auto OnTrackUpdate() -> void;
  auto OnPlaybackUpdate(uint32_t, uint32_t) -> void;
  auto OnQueueUpdate() -> void;

  auto OnFocusAboveFold() -> void;
  auto OnFocusBelowFold() -> void;

 private:
  auto control_button(lv_obj_t* parent, char* icon) -> lv_obj_t*;
  auto next_up_label(lv_obj_t* parent, const std::string& text) -> lv_obj_t*;

  auto BindTrack(const database::Track& track) -> void;
  auto ApplyNextUp(const std::vector<database::Track>& tracks) -> void;

  std::weak_ptr<database::Database> db_;
  audio::TrackQueue* queue_;

  std::optional<database::Track> track_;
  std::vector<database::Track> next_tracks_;

  std::unique_ptr<database::FutureFetcher<std::optional<database::Track>>>
      new_track_;
  std::unique_ptr<
      database::FutureFetcher<std::vector<std::optional<database::Track>>>>
      new_next_tracks_;

  lv_obj_t* artist_label_;
  lv_obj_t* album_label_;
  lv_obj_t* title_label_;

  lv_obj_t* scrubber_;
  lv_obj_t* play_pause_control_;

  lv_obj_t* next_up_header_;
  lv_obj_t* next_up_label_;
  lv_obj_t* next_up_hint_;
  lv_obj_t* next_up_container_;
};

}  // namespace screens
}  // namespace ui
