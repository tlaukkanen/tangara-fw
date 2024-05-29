/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <optional>
#include <string>
#include <variant>

namespace tts {

/*
 * 'Simple' TTS events are events that do not have any extra event-specific
 * details.
 */
enum class SimpleEvent {
  /*
   * Indicates that the screen's content has substantially changed. e.g. a new
   * screen has been pushed.
   */
  kContextChanged,
};

/*
 * Event indicating that the currently selected LVGL object has changed.
 */
struct SelectionChanged {
  struct Selection {
    std::optional<std::string> description;
    bool is_interactive;
  };

  std::optional<Selection> new_selection;
};

using Event = std::variant<SimpleEvent, SelectionChanged>;

}  // namespace tts
