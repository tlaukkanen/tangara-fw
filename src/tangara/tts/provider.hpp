/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "tts/events.hpp"
#include "tts/player.hpp"

namespace tts {

/*
 * A TTS Provider is responsible for receiving system events that may be
 * relevant to TTS, and digesting them into discrete 'utterances' that can be
 * used to generate audio feedback.
 */
class Provider {
 public:
  Provider();

  auto player(std::unique_ptr<Player>) -> void;

  auto feed(const Event&) -> void;

  // Not copyable or movable.
  Provider(const Provider&) = delete;
  Provider& operator=(const Provider&) = delete;

 private:
  std::unique_ptr<Player> player_;
};

}  // namespace tts
