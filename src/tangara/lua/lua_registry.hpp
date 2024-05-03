/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <string>

#include "lua.hpp"

#include "lua/bridge.hpp"
#include "lua/lua_thread.hpp"
#include "system_fsm/service_locator.hpp"

namespace lua {

class Registry {
 public:
  static auto instance(system_fsm::ServiceLocator&) -> Registry&;

  auto uiThread() -> std::shared_ptr<LuaThread>;
  auto newThread() -> std::shared_ptr<LuaThread>;

  auto AddPropertyModule(
      const std::string&,
      std::vector<std::pair<std::string, std::variant<LuaFunction, Property*>>>)
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
