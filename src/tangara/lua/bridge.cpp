/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua/bridge.hpp"
#include <stdint.h>
#include <stdio.h>
#include <sys/unistd.h>

#include <cstdint>
#include <memory>
#include <string>

#include "database/database.hpp"
#include "database/index.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "font/lv_binfont_loader.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua.hpp"
#include "lua/lua_controls.hpp"
#include "lua/lua_database.hpp"
#include "lua/lua_filesystem.hpp"
#include "lua/lua_font.hpp"
#include "lua/lua_queue.hpp"
#include "lua/lua_screen.hpp"
#include "lua/lua_testing.hpp"
#include "lua/lua_theme.hpp"
#include "lua/lua_version.hpp"
#include "lvgl.h"

#include "luavgl.h"

#include "events/event_queue.hpp"
#include "lua/property.hpp"
#include "misc/lv_fs.h"
#include "system_fsm/service_locator.hpp"
#include "ui/ui_events.hpp"

extern "C" {
int luaopen_linenoise(lua_State* L);
int luaopen_term_core(lua_State* L);
}

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_bridge";

static constexpr char kBridgeKey[] = "bridge";

static auto delete_font_cb(const lv_font_t* font) -> void {
  // FIXME: luavgl never actually calls this?
}

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
  RegisterTestingModule(L);
  RegisterFileSystemModule(L);
  RegisterVersionModule(L);
  RegisterThemeModule(L);
  RegisterScreenModule(L);
}

auto Bridge::installLvgl(lua_State* L) -> void {
  luavgl_set_pcall(L, CallProtected);
  luavgl_set_font_extension(L, loadFont, delete_font_cb);
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
