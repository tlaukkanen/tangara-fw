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

using LuaValue = std::variant<std::monostate, int, float, bool, std::string>;

class Property {
 public:
  Property() : Property(std::monostate{}) {}
  Property(const LuaValue&);
  Property(const LuaValue&, std::function<bool(const LuaValue&)>);

  auto IsTwoWay() -> bool { return cb_.has_value(); }

  auto PushValue(lua_State& s) -> int;
  auto PopValue(lua_State& s) -> bool;
  auto Update(const LuaValue& new_val) -> void;

  auto AddLuaBinding(lua_State*, int ref) -> void;

 private:
  LuaValue value_;
  std::optional<std::function<bool(const LuaValue&)>> cb_;
  std::vector<std::pair<lua_State*, int>> bindings_;
};

class PropertyBindings {
 public:
  PropertyBindings(lua_State&);

  auto Register(lua_State*, Property*) -> void;
};

}  // namespace lua
