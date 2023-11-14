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

class LuaThread {
 public:
  static auto Start(system_fsm::ServiceLocator&, lv_obj_t* lvgl_root = nullptr)
      -> LuaThread*;
  ~LuaThread();

  auto RunScript(const std::string& path) -> bool;

  auto bridge() -> Bridge& { return *bridge_; }

 private:
  LuaThread(std::unique_ptr<Allocator>&, std::unique_ptr<Bridge>&, lua_State*);

  std::unique_ptr<Allocator> alloc_;
  std::unique_ptr<Bridge> bridge_;
  lua_State* state_;
};

}  // namespace lua
