/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui/screen_lua.hpp"

#include "core/lv_obj_tree.h"
#include "lua.h"
#include "lua.hpp"
#include "lua/property.hpp"
#include "ui/themes.hpp"

#include "lua/lua_thread.hpp"
#include "luavgl.h"

namespace ui {
namespace screens {

Lua::Lua() : s_(nullptr), obj_ref_() {
  themes::Theme::instance()->ApplyStyle(root_, "root");
}

Lua::~Lua() {
  if (s_ && obj_ref_) {
    luaL_unref(s_, LUA_REGISTRYINDEX, *obj_ref_);
  }
}

auto Lua::onShown() -> void {
  callMethod("on_show");
  forEachBinding([&](lua::Binding* b) {
    b->active = true;
    lua::Binding::apply(s_, -1);
  });
}

auto Lua::onHidden() -> void {
  callMethod("on_hide");
  forEachBinding([&](lua::Binding* b) { b->active = false; });
}

auto Lua::canPop() -> bool {
  if (!s_ || !obj_ref_) {
    return true;
  }
  lua_rawgeti(s_, LUA_REGISTRYINDEX, *obj_ref_);
  lua_pushliteral(s_, "can_pop");

  if (lua_gettable(s_, -2) == LUA_TFUNCTION) {
    // If we got a callback instead of a value, then invoke it to turn it into
    // value.
    lua_pushvalue(s_, -2);
    lua::CallProtected(s_, 1, 1);
  }
  bool ret = lua_toboolean(s_, -1);

  lua_pop(s_, 2);
  return ret;
}

auto Lua::SetObjRef(lua_State* s) -> void {
  assert(s_ == nullptr);
  s_ = s;
  obj_ref_ = luaL_ref(s, LUA_REGISTRYINDEX);
}

auto Lua::callMethod(std::string name) -> void {
  if (!s_ || !obj_ref_) {
    return;
  }
  lua_rawgeti(s_, LUA_REGISTRYINDEX, *obj_ref_);
  lua_pushlstring(s_, name.data(), name.size());

  if (lua_gettable(s_, -2) == LUA_TFUNCTION) {
    lua_pushvalue(s_, -2);
    lua::CallProtected(s_, 1, 0);
  } else {
    lua_pop(s_, 1);
  }

  lua_pop(s_, 1);
}

auto Lua::forEachBinding(std::function<void(lua::Binding*)> fn) -> void {
  if (!s_ || !obj_ref_) {
    return;
  }
  lua_rawgeti(s_, LUA_REGISTRYINDEX, *obj_ref_);
  lua_pushliteral(s_, "bindings");

  if (lua_gettable(s_, -2) != LUA_TTABLE) {
    lua_pop(s_, 2);
    return;
  }

  lua_pushnil(s_);
  while (lua_next(s_, -2) != 0) {
    lua::Binding* b = lua::Binding::get(s_, -1);
    if (b) {
      std::invoke(fn, b);
    }
    lua_pop(s_, 1);
  }

  lua_pop(s_, 2);
}

}  // namespace screens
}  // namespace ui
