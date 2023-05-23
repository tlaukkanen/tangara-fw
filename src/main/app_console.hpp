/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "audio_playback.hpp"
#include "console.hpp"
#include "database.hpp"
#include "storage.hpp"

namespace console {

class AppConsole : public Console {
 public:
  explicit AppConsole(audio::AudioPlayback* playback,
                      database::Database* database);
  virtual ~AppConsole();

  audio::AudioPlayback* playback_;
  database::Database* database_;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
