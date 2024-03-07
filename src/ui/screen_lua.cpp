/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_lua.hpp"

#include "core/lv_obj_tree.h"
#include "lua.h"
#include "lua.hpp"

#include "lua_thread.hpp"
#include "luavgl.h"

namespace ui {
namespace screens {

Lua::Lua() : s_(nullptr), obj_ref_() {}

Lua::~Lua() {
  if (s_ && obj_ref_) {
    luaL_unref(s_, LUA_REGISTRYINDEX, *obj_ref_);
  }
}

auto Lua::onShown() -> void {
  if (!s_ || !obj_ref_) {
    return;
  }
  lua_rawgeti(s_, LUA_REGISTRYINDEX, *obj_ref_);
  lua_pushliteral(s_, "onShown");

  if (lua_gettable(s_, -2) == LUA_TFUNCTION) {
    lua_pushvalue(s_, -2);
    lua::CallProtected(s_, 1, 0);
  } else {
    lua_pop(s_, 1);
  }

  lua_pop(s_, 1);
}

auto Lua::onHidden() -> void {
  if (!s_ || !obj_ref_) {
    return;
  }
  lua_rawgeti(s_, LUA_REGISTRYINDEX, *obj_ref_);
  lua_pushliteral(s_, "onHidden");

  if (lua_gettable(s_, -2) == LUA_TFUNCTION) {
    lua_pushvalue(s_, -2);
    lua::CallProtected(s_, 1, 0);
  } else {
    lua_pop(s_, 1);
  }

  lua_pop(s_, 1);
}

auto Lua::SetObjRef(lua_State* s) -> void {
  assert(s_ == nullptr);
  s_ = s;
  obj_ref_ = luaL_ref(s, LUA_REGISTRYINDEX);
}

}  // namespace screens
}  // namespace ui
