/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "lua.hpp"

namespace lua {

auto RegisterVersionModule(lua_State*) -> void;

}  // namespace lua