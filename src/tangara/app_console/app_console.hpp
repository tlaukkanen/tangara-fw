/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "audio/track_queue.hpp"
#include "bluetooth.hpp"
#include "dev_console/console.hpp"
#include "database/database.hpp"
#include "samd.hpp"
#include "system_fsm/service_locator.hpp"

namespace console {

class AppConsole : public Console {
 public:
  static std::shared_ptr<system_fsm::ServiceLocator> sServices;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
