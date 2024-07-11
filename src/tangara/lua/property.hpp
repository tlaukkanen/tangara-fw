/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <memory>
#include <string>

#include "audio/audio_events.hpp"
#include "drivers/bluetooth_types.hpp"
#include "lua.hpp"
#include "lvgl.h"
#include "system_fsm/service_locator.hpp"

namespace lua {

// FIXME: We should use some kind of interface for this instead.
using LuaValue = std::variant<std::monostate,
                              int,
                              bool,
                              std::string,
                              audio::TrackInfo,
                              drivers::bluetooth::MacAndName,
                              std::vector<drivers::bluetooth::MacAndName>>;

using LuaFunction = std::function<int(lua_State*)>;

class Property {
 public:
  Property() : Property(std::monostate{}) {}
  Property(const LuaValue&);
  Property(const LuaValue&, std::function<bool(const LuaValue&)> filter);

  auto get() -> const LuaValue& { return *value_; }

  /*
   * Assigns a new value to this property, bypassing the filter fn. All
   * bindings will be marked as dirty, and if active, will be reapplied.
   */
  auto setDirect(const LuaValue&) -> void;
  /*
   * Invokes the filter fn, and if successful, assigns the new value to this
   * property. All bindings will be marked as dirty, and if active, will be
   * reapplied.
   */
  auto set(const LuaValue&) -> bool;

  /* Returns whether or not this Property can be written from Lua. */
  auto isTwoWay() -> bool { return cb_.has_value(); }

  auto pushValue(lua_State& s) -> int;
  auto popValue(lua_State& s) -> bool;

  /* Reapplies all active, dirty bindings associated with this Property. */
  auto reapplyAll() -> void;

  auto addLuaBinding(lua_State*, int ref) -> void;
  auto applySingle(lua_State*, int ref, bool mark_dirty) -> bool;

 private:
  std::unique_ptr<LuaValue> value_;
  std::optional<std::function<bool(const LuaValue&)>> cb_;
  std::pmr::vector<std::pair<lua_State*, int>> bindings_;
};

/*
 * Container for a Lua function that should be invoked whenever a Property's
 * value changes, as well as some extra accounting metadata.
 */
struct Binding {
  /* Checks the value at idx is a Binding, returning a pointer to it if so. */
  static auto get(lua_State*, int idx) -> Binding*;
  /*
   * If the value at idx is a dirty, active Binding, applies the current value
   * from its Property. Returns false if the binding was active and dirty, but
   * invoking the Lua callback failed.
   */
  static auto apply(lua_State*, int idx) -> bool;

  Property* property;
  bool active;
  bool dirty;
};

static_assert(std::is_trivially_destructible<Binding>());
static_assert(std::is_trivially_copy_assignable<Binding>());

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
