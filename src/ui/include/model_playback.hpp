/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "bindey/property.h"

#include "track.hpp"

namespace ui {
namespace models {

struct Playback {
  bindey::property<bool> is_playing;
  bindey::property<std::optional<database::TrackId>> current_track;
  bindey::property<std::vector<database::TrackId>> upcoming_tracks;

  bindey::property<uint32_t> current_track_position;
  bindey::property<uint32_t> current_track_duration;
};

}  // namespace models
}  // namespace ui