/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "console.hpp"
#include "database.hpp"
#include "track_queue.hpp"

namespace console {

class AppConsole : public Console {
 public:
  static std::weak_ptr<database::Database> sDatabase;
  static audio::TrackQueue* sTrackQueue;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
