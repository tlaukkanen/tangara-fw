/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "core/lv_obj.h"
#include "drivers/haptics.hpp"

#include "input/feedback_device.hpp"
#include "tts/events.hpp"
#include "tts/provider.hpp"

namespace input {

class TextToSpeech : public IFeedbackDevice {
 public:
  TextToSpeech(tts::Provider&);

  auto feedback(lv_group_t*, uint8_t event_type) -> void override;

 private:
  tts::Provider& tts_;

  auto describe(lv_obj_t&) -> void;
  auto findDescription(lv_obj_t&) -> std::optional<std::string>;

  lv_group_t* last_group_;
  lv_obj_t* last_obj_;
};

}  // namespace input
