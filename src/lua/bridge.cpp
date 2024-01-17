/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bridge.hpp"
#include <sys/_stdint.h>

#include <memory>
#include <string>

#include "database.hpp"
#include "esp_log.h"
#include "index.hpp"
#include "lauxlib.h"
#include "lua.h"
#include "lua.hpp"
#include "lua_controls.hpp"
#include "lua_database.hpp"
#include "lua_queue.hpp"
#include "lua_version.hpp"
#include "lvgl.h"

#include "event_queue.hpp"
#include "property.hpp"
#include "service_locator.hpp"
#include "ui_events.hpp"

extern "C" {
int luaopen_linenoise(lua_State* L);
int luaopen_term_core(lua_State* L);
}

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_bridge";

static constexpr char kBridgeKey[] = "bridge";

auto Bridge::Get(lua_State* state) -> Bridge* {
  lua_pushstring(state, kBridgeKey);
  lua_gettable(state, LUA_REGISTRYINDEX);
  return reinterpret_cast<Bridge*>(lua_touserdata(state, -1));
}

Bridge::Bridge(system_fsm::ServiceLocator& services, lua_State& s)
    : services_(services), state_(s), bindings_(s) {
  lua_pushstring(&s, kBridgeKey);
  lua_pushlightuserdata(&s, this);
  lua_settable(&s, LUA_REGISTRYINDEX);

  luaL_requiref(&s, "linenoise", luaopen_linenoise, true);
  lua_pop(&s, 1);

  luaL_requiref(&s, "term.core", luaopen_term_core, true);
  lua_pop(&s, 1);

  RegisterControlsModule(&s);
  RegisterDatabaseModule(&s);
  RegisterQueueModule(&s);
  RegisterVersionModule(&s);
}

static auto new_property_module(lua_State* state) -> int {
  const char* name = luaL_checkstring(state, 1);
  luaL_newmetatable(state, name);

  lua_pushstring(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);  // metatable.__index = metatable

  return 1;
}

template <class... Ts>
inline constexpr bool always_false_v = false;

auto Bridge::AddPropertyModule(
    const std::string& name,
    std::vector<std::pair<std::string, std::variant<LuaFunction, Property*>>>
        props) -> void {
  // Create the module (or retrieve it if one with this name already exists)
  luaL_requiref(&state_, name.c_str(), new_property_module, true);

  for (const auto& prop : props) {
    lua_pushstring(&state_, prop.first.c_str());
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, LuaFunction>) {
            bindings_.Register(&state_, arg);
          } else if constexpr (std::is_same_v<T, Property*>) {
            bindings_.Register(&state_, arg);
          } else {
            static_assert(always_false_v<T>, "missing case");
          }
        },
        prop.second);

    lua_settable(&state_, -3);  // metatable.propname = property
  }

  lua_pop(&state_, 1);  // pop the module off the stack
}

}  // namespace lua
