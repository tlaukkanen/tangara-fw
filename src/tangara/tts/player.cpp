/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tts/player.hpp"

#include "esp_log.h"

namespace tts {

[[maybe_unused]] static constexpr char kTag[] = "ttsplay";

Player::Player(tasks::WorkerPool& worker,
               drivers::PcmBuffer& output,
               audio::FatfsStreamFactory& factory)
    : bg_(worker), stream_factory_(factory), output_(output) {}

auto Player::playFile(const std::string& path) -> void {
  ESP_LOGI(kTag, "playing '%s'", path.c_str());
}

}  // namespace tts
