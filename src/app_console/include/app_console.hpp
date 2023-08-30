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
#include "service_locator.hpp"
#include "track_queue.hpp"

namespace console {

class AppConsole : public Console {
 public:
  static std::shared_ptr<system_fsm::ServiceLocator> sServices;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
