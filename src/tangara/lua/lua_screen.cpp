/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_screen.hpp"

#include <memory>
#include <string>

#include "lua.hpp"

#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lvgl.h"

#include "bridge.hpp"
#include "database.hpp"
#include "event_queue.hpp"
#include "index.hpp"
#include "property.hpp"
#include "service_locator.hpp"
#include "track.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"

namespace lua {

static auto screen_new(lua_State* L) -> int {
  // o = o or {}
  if (lua_gettop(L) != 2) {
    lua_settop(L, 1);
    lua_newtable(L);
  }
  // Swap o and self on the stack.
  lua_insert(L, 1);

  lua_pushliteral(L, "__index");
  lua_pushvalue(L, 1);
  lua_settable(L, 1);  // self.__index = self

  lua_setmetatable(L, 1);  // setmetatable(o, self)

  return 1;  // return o
}

static auto screen_noop(lua_State* state) -> int {
  return 0;
}

static auto screen_true(lua_State* state) -> int {
  lua_pushboolean(state, true);
  return 1;
}

static const struct luaL_Reg kScreenFuncs[] = {
    {"new", screen_new},      {"createUi", screen_noop},
    {"onShown", screen_noop}, {"onHidden", screen_noop},
    {"canPop", screen_true},  {NULL, NULL}};

static auto lua_screen(lua_State* state) -> int {
  luaL_newlib(state, kScreenFuncs);

  lua_pushliteral(state, "__index");
  lua_pushvalue(state, -2);
  lua_rawset(state, -3);

  return 1;
}

auto RegisterScreenModule(lua_State* s) -> void {
  luaL_requiref(s, "screen", lua_screen, true);

  lua_pop(s, 1);
}

}  // namespace lua
