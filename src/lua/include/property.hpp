/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <memory>
#include <string>

#include "audio_events.hpp"
#include "bluetooth_types.hpp"
#include "lua.hpp"
#include "lvgl.h"
#include "service_locator.hpp"

namespace lua {

// FIXME: We should use some kind of interface for this instead.
using LuaValue = std::variant<std::monostate,
                              int,
                              bool,
                              std::string,
                              audio::TrackInfo,
                              drivers::bluetooth::Device,
                              std::vector<drivers::bluetooth::Device>>;

using LuaFunction = std::function<int(lua_State*)>;

class Property {
 public:
  Property() : Property(std::monostate{}) {}
  Property(const LuaValue&);
  Property(const LuaValue&, std::function<bool(const LuaValue&)>);

  auto Get() -> const LuaValue& { return *value_; }

  auto IsTwoWay() -> bool { return cb_.has_value(); }

  auto PushValue(lua_State& s) -> int;
  auto PopValue(lua_State& s) -> bool;
  auto Update(const LuaValue& new_val) -> void;

  auto AddLuaBinding(lua_State*, int ref) -> void;

 private:
  std::unique_ptr<LuaValue> value_;
  std::optional<std::function<bool(const LuaValue&)>> cb_;
  std::pmr::vector<std::pair<lua_State*, int>> bindings_;
};

class PropertyBindings {
 public:
  PropertyBindings();

  auto install(lua_State*) -> void;

  auto Register(lua_State*, Property*) -> void;
  auto Register(lua_State*, LuaFunction) -> void;

  auto GetFunction(size_t i) -> const LuaFunction&;

 private:
  std::pmr::vector<LuaFunction> functions_;
};

}  // namespace lua
