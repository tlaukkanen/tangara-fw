/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "lua.hpp"

#include "database/database.hpp"

namespace lua {

auto db_check_iterator(lua_State*, int stack_pos) -> database::Iterator*;

auto pushTagValue(lua_State* L, const database::TagValue& val) -> void; 

auto RegisterDatabaseModule(lua_State*) -> void;

}  // namespace lua
