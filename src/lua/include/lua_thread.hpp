/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <string>

#include "lua.hpp"
#include "lvgl.h"

#include "bridge.hpp"
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

class Registry {
 public:
  static auto instance(system_fsm::ServiceLocator&) -> Registry&;

  auto uiThread() -> std::shared_ptr<LuaThread>;
  auto newThread() -> std::shared_ptr<LuaThread>;

  auto AddPropertyModule(
      const std::string&,
      std::vector<
          std::pair<std::string, std::variant<LuaFunction, Property*>>>)
      -> void;

  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;

 private:
  Registry(system_fsm::ServiceLocator&);

  system_fsm::ServiceLocator& services_;
  std::unique_ptr<Bridge> bridge_;

  std::shared_ptr<LuaThread> ui_thread_;
  std::list<std::weak_ptr<LuaThread>> threads_;

  std::vector<
      std::pair<std::string,
                std::vector<std::pair<std::string,
                                      std::variant<LuaFunction, Property*>>>>>
      modules_;
};

}  // namespace lua
