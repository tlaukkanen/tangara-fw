/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "lua.hpp"

#include "screen.hpp"

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
  lua_State* s_;
  std::optional<int> obj_ref_;
};

}  // namespace screens
}  // namespace ui
