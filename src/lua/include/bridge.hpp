/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <string>

#include "lua.hpp"
#include "lvgl.h"
#include "service_locator.hpp"

namespace lua {

class Bridge {
 public:
  Bridge(system_fsm::ServiceLocator&, lua_State& s);

  system_fsm::ServiceLocator& services() { return services_; }

 private:
  system_fsm::ServiceLocator& services_;
  lua_State& state_;
};

}  // namespace lua
