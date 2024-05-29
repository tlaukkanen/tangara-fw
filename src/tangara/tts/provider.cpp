/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tts/provider.hpp"

#include <optional>
#include <string>
#include <variant>

#include "esp_log.h"

#include "tts/events.hpp"

namespace tts {

[[maybe_unused]] static constexpr char kTag[] = "tts";

Provider::Provider() {}

auto Provider::feed(const Event& e) -> void {
  if (std::holds_alternative<SimpleEvent>(e)) {
    // ESP_LOGI(kTag, "context changed");
  } else if (std::holds_alternative<SelectionChanged>(e)) {
    auto ev = std::get<SelectionChanged>(e);
    if (!ev.new_selection) {
      // ESP_LOGI(kTag, "no selection");
    } else {
      // ESP_LOGI(kTag, "new selection: '%s', interactive? %i",
      // ev.new_selection->description.value_or("").c_str(),
      // ev.new_selection->is_interactive);
    }
  }
}

}  // namespace tts
