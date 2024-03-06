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
#include "lua_screen.hpp"
#include "lua_version.hpp"
#include "lvgl.h"

#include "font/lv_font_loader.h"
#include "luavgl.h"

#include "event_queue.hpp"
#include "property.hpp"
#include "service_locator.hpp"
#include "ui_events.hpp"

extern "C" {
int luaopen_linenoise(lua_State* L);
int luaopen_term_core(lua_State* L);
}

LV_FONT_DECLARE(font_fusion_12);
LV_FONT_DECLARE(font_fusion_10);

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_bridge";

static constexpr char kBridgeKey[] = "bridge";

static auto make_font_cb(const char* name, int size, int weight)
    -> const lv_font_t* {
  if (std::string{"fusion"} == name) {
    if (size == 12) {
      return &font_fusion_12;
    }
    if (size == 10) {
      return &font_fusion_10;
    }
  }
  return NULL;
}

static auto delete_font_cb(lv_font_t* font) -> void {}

auto Bridge::Get(lua_State* state) -> Bridge* {
  lua_pushstring(state, kBridgeKey);
  lua_gettable(state, LUA_REGISTRYINDEX);
  return reinterpret_cast<Bridge*>(lua_touserdata(state, -1));
}

Bridge::Bridge(system_fsm::ServiceLocator& services) : services_(services) {}

auto Bridge::installBaseModules(lua_State* L) -> void {
  lua_pushstring(L, kBridgeKey);
  lua_pushlightuserdata(L, this);
  lua_settable(L, LUA_REGISTRYINDEX);

  bindings_.install(L);

  luaL_requiref(L, "linenoise", luaopen_linenoise, true);
  lua_pop(L, 1);

  luaL_requiref(L, "term.core", luaopen_term_core, true);
  lua_pop(L, 1);

  RegisterControlsModule(L);
  RegisterDatabaseModule(L);
  RegisterQueueModule(L);
  RegisterVersionModule(L);
  RegisterScreenModule(L);
}

auto Bridge::installLvgl(lua_State* L) -> void {
  luavgl_set_pcall(L, CallProtected);
  luavgl_set_font_extension(L, make_font_cb, delete_font_cb);
  luaL_requiref(L, "lvgl", luaopen_lvgl, true);
  lua_pop(L, 1);
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

auto Bridge::installPropertyModule(
    lua_State* L,
    const std::string& name,
    std::vector<std::pair<std::string, std::variant<LuaFunction, Property*>>>&
        props) -> void {
  // Create the module (or retrieve it if one with this name already exists)
  luaL_requiref(L, name.c_str(), new_property_module, true);

  for (const auto& prop : props) {
    lua_pushstring(L, prop.first.c_str());
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, LuaFunction>) {
            bindings_.Register(L, arg);
          } else if constexpr (std::is_same_v<T, Property*>) {
            bindings_.Register(L, arg);
          } else {
            static_assert(always_false_v<T>, "missing case");
          }
        },
        prop.second);

    lua_settable(L, -3);  // metatable.propname = property
  }

  lua_pop(L, 1);  // pop the module off the stack
}

}  // namespace lua
