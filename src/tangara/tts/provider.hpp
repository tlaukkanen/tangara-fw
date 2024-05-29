/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <optional>
#include <string>
#include <variant>

#include "tts/events.hpp"

namespace tts {

class Provider {
 public:
  Provider();
  auto feed(const Event&) -> void;
};

}  // namespace tts
