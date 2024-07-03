/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "lua.hpp"

namespace lua {

auto loadFont(lua_State* L, const char* name, int cb_ref) -> void;

}
