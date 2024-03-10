/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_lua.hpp"

#include "core/lv_obj_tree.h"
#include "lua.hpp"
#include "themes.hpp"

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

auto Lua::SetObjRef(lua_State* s) -> void {
  assert(s_ == nullptr);
  s_ = s;
  obj_ref_ = luaL_ref(s, LUA_REGISTRYINDEX);
}

}  // namespace screens
}  // namespace ui
