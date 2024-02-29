/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <string>

#include "lua.hpp"

#include "service_locator.hpp"

namespace lua {

class Allocator;

auto CallProtected(lua_State*, int nargs, int nresults) -> int;

class LuaThread {
 public:
  static auto Start(system_fsm::ServiceLocator&) -> LuaThread*;
  ~LuaThread();

  auto RunScript(const std::string& path) -> bool;
  auto RunString(const std::string& path) -> bool;

  auto DumpStack() -> void;

  auto state() -> lua_State* { return state_; }

  LuaThread(const LuaThread&) = delete;
  LuaThread& operator=(const LuaThread&) = delete;

 private:
  LuaThread(std::unique_ptr<Allocator>&, lua_State*);

  std::unique_ptr<Allocator> alloc_;
  lua_State* state_;
};

}  // namespace lua
