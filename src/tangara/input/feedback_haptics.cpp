/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/feedback_haptics.hpp"

#include <cstdint>

#include "core/lv_group.h"
#include "esp_log.h"
#include "lvgl/lvgl.h"

#include "drivers/haptics.hpp"

namespace input {

using Effect = drivers::Haptics::Effect;

Haptics::Haptics(drivers::Haptics& haptics_) : haptics_(haptics_) {}

auto Haptics::feedback(lv_group_t* group, uint8_t event_type) -> void {
  lv_obj_t* obj = lv_group_get_focused(group);
  if (obj == last_selection_) {
    return;
  }
  last_selection_ = obj;

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
