/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "battery.hpp"
#include "bindey/property.h"

#include "track.hpp"

namespace ui {
namespace models {

struct TopBar {
  bindey::property<battery::Battery::BatteryState> battery_state;

  // Shared with the Playback model
  bindey::property<bool>& is_playing;
  bindey::property<std::optional<database::TrackId>>& current_track;
};

}  // namespace models
}  // namespace ui
