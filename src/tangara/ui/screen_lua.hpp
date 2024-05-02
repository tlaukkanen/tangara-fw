/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "lua.hpp"

#include "lua/property.hpp"
#include "ui/screen.hpp"

namespace ui {
namespace screens {

class Lua : public Screen {
 public:
  Lua();
  ~Lua();

  auto onShown() -> void override;
  auto onHidden() -> void override;

  auto canPop() -> bool override;

  auto SetObjRef(lua_State*) -> void;

 private:
  /* Invokes a method on this screen's Lua counterpart. */
  auto callMethod(std::string name) -> void;
  /* Applies fn to each binding in this screen's `bindings` field. */
  auto forEachBinding(std::function<void(lua::Binding*)> fn) -> void;

  lua_State* s_;
  std::optional<int> obj_ref_;
};

}  // namespace screens
}  // namespace ui
