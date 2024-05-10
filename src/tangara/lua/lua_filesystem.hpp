/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include "lua.hpp"
#include "lua/file_iterator.hpp"

namespace lua {

auto check_file_iterator(lua_State*, int stack_pos) -> database::FileIterator*;

auto RegisterFileSystemModule(lua_State*) -> void;

}  // namespace lua
