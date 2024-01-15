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
#include "property.hpp"
#include "service_locator.hpp"

namespace lua {

class Bridge {
 public:
  static auto Get(lua_State* state) -> Bridge*;

  Bridge(system_fsm::ServiceLocator&, lua_State& s);

  auto AddPropertyModule(
      const std::string&,
      std::vector<
          std::pair<std::string,
                    std::variant<LuaFunction, Property*>>>)
      -> void;

  system_fsm::ServiceLocator& services() { return services_; }
  PropertyBindings& bindings() { return bindings_; }

 private:
  system_fsm::ServiceLocator& services_;
  lua_State& state_;
  PropertyBindings bindings_;
};

}  // namespace lua
