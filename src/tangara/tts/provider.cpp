/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tts/provider.hpp"
#include <stdint.h>

#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

#include "drivers/storage.hpp"
#include "esp_log.h"

#include "komihash.h"
#include "tts/events.hpp"

namespace tts {

[[maybe_unused]] static constexpr char kTag[] = "tts";

static const char* kTtsPath = "/.tangara-tts/";

static auto textToFile(const std::string& text) -> std::optional<std::string> {
  uint64_t hash = komihash(text.data(), text.size(), 0);
  std::stringstream stream;
  stream << kTtsPath << std::hex << hash << ".wav";
  return stream.str();
}

Provider::Provider() {}

auto Provider::player(std::unique_ptr<Player> p) -> void {
  player_ = std::move(p);
}

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
      std::string new_desc = ev.new_selection->description.value_or("");
      if (player_) {
        player_->playFile(textToFile(new_desc).value_or(""));
      }
    }
  }
}

}  // namespace tts
