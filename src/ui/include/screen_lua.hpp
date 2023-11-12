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
  explicit Lua(lua_State* l);
};

}  // namespace screens
}  // namespace ui
