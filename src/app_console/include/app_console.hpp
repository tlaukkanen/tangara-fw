/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "bluetooth.hpp"
#include "console.hpp"
#include "database.hpp"
#include "samd.hpp"
#include "track_queue.hpp"

namespace console {

class AppConsole : public Console {
 public:
  static std::weak_ptr<database::Database> sDatabase;
  static audio::TrackQueue* sTrackQueue;
  static drivers::Bluetooth* sBluetooth;
  static drivers::Samd* sSamd;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
