/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "feedback_haptics.hpp"

#include <cstdint>

#include "lvgl/lvgl.h"

#include "core/lv_event.h"
#include "esp_log.h"

#include "haptics.hpp"

namespace input {

using Effect = drivers::Haptics::Effect;

Haptics::Haptics(drivers::Haptics& haptics_) : haptics_(haptics_) {}

auto Haptics::feedback(uint8_t event_type) -> void {
  switch (event_type) {
    case LV_EVENT_FOCUSED:
      haptics_.PlayWaveformEffect(Effect::kMediumClick1_100Pct);
      break;
    case LV_EVENT_CLICKED:
      haptics_.PlayWaveformEffect(Effect::kSharpClick_100Pct);
      break;
    default:
      break;
  }
}

}  // namespace input
