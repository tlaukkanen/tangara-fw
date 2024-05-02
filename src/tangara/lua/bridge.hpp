/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <string>

#include "lua.hpp"
#include "lua/property.hpp"
#include "lvgl.h"
#include "system_fsm/service_locator.hpp"

namespace lua {

/*
 * Responsible for adding C/C++ module bindings to Lua threads. This class
 * keeps no thread-specific internal state, and instead uses the LUA_REGISTRY
 * table of its host threads to store data.
 */
class Bridge {
 public:
  /*
   * Utility for retrieving the Bridge from a Lua thread in which the Bridge's
   * bindings have been installed. Use by Lua's C callbacks to access the rest
   * of the system.
   */
  static auto Get(lua_State* state) -> Bridge*;

  Bridge(system_fsm::ServiceLocator& s);

  system_fsm::ServiceLocator& services() { return services_; }

  auto installBaseModules(lua_State* L) -> void;
  auto installLvgl(lua_State* L) -> void;
  auto installPropertyModule(
      lua_State* L,
      const std::string&,
      std::vector<std::pair<std::string,
                            std::variant<LuaFunction, Property*>>>&) -> void;

  Bridge(const Bridge&) = delete;
  Bridge& operator=(const Bridge&) = delete;

 private:
  system_fsm::ServiceLocator& services_;
  PropertyBindings bindings_;
};

}  // namespace lua
