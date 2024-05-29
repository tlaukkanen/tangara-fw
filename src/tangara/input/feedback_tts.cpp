/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/feedback_tts.hpp"

#include <cstdint>
#include <variant>

#include "lvgl/lvgl.h"

#include "core/lv_event.h"
#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_class.h"
#include "core/lv_obj_tree.h"
#include "extra/widgets/list/lv_list.h"
#include "tts/events.hpp"
#include "widgets/lv_label.h"

#include "tts/events.hpp"
#include "tts/provider.hpp"

namespace input {

TextToSpeech::TextToSpeech(tts::Provider& tts)
    : tts_(tts), last_obj_(nullptr) {}

auto TextToSpeech::feedback(lv_group_t* group, uint8_t event_type) -> void {
  if (group != last_group_) {
    last_group_ = group;
    last_obj_ = nullptr;
    if (group) {
      tts_.feed(tts::SimpleEvent::kContextChanged);
    }
  }

  if (group) {
    lv_obj_t* focused = lv_group_get_focused(group);
    if (focused == last_obj_) {
      return;
    }

    last_obj_ = focused;
    if (focused != nullptr) {
      describe(*focused);
    }
  }
}

auto TextToSpeech::describe(lv_obj_t& obj) -> void {
  if (lv_obj_check_type(&obj, &lv_btn_class) ||
      lv_obj_check_type(&obj, &lv_list_btn_class)) {
    auto desc = findDescription(obj);
    tts_.feed(tts::SelectionChanged{
        .new_selection =
            tts::SelectionChanged::Selection{
                .description = desc,
                .is_interactive = true,
            },
    });
  } else {
    auto desc = findDescription(obj);
    tts_.feed(tts::SelectionChanged{
        .new_selection =
            tts::SelectionChanged::Selection{
                .description = desc,
                .is_interactive = false,
            },
    });
  }
}

auto TextToSpeech::findDescription(lv_obj_t& obj)
    -> std::optional<std::string> {
  if (lv_obj_get_child_cnt(&obj) > 0) {
    for (size_t i = 0; i < lv_obj_get_child_cnt(&obj); i++) {
      auto res = findDescription(*lv_obj_get_child(&obj, i));
      if (res) {
        return res;
      }
    }
  }

  if (lv_obj_check_type(&obj, &lv_label_class)) {
    std::string text = lv_label_get_text(&obj);
    if (!text.empty()) {
      return text;
    }
  }

  return {};
}

}  // namespace input
