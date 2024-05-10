/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua/lua_registry.hpp"

#include <iostream>
#include <memory>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lua.hpp"

#include "events/event_queue.hpp"
#include "lua/bridge.hpp"
#include "memory_resource.hpp"
#include "system_fsm/service_locator.hpp"
#include "ui/ui_events.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua";

auto Registry::instance(system_fsm::ServiceLocator& s) -> Registry& {
  static Registry sRegistry{s};
  return sRegistry;
}

Registry::Registry(system_fsm::ServiceLocator& services)
    : services_(services), bridge_(new Bridge(services)) {}

auto Registry::uiThread() -> std::shared_ptr<LuaThread> {
  if (!ui_thread_) {
    ui_thread_ = newThread();
    bridge_->installLvgl(ui_thread_->state());
  }
  return ui_thread_;
}

auto Registry::newThread() -> std::shared_ptr<LuaThread> {
  std::shared_ptr<LuaThread> thread{LuaThread::Start(services_)};
  bridge_->installBaseModules(thread->state());
  for (auto& module : modules_) {
    bridge_->installPropertyModule(thread->state(), module.first,
                                   module.second);
  }
  threads_.push_back(thread);
  return thread;
}

auto Registry::AddPropertyModule(
    const std::string& name,
    std::vector<std::pair<std::string, std::variant<LuaFunction, Property*>>>
        properties) -> void {
  modules_.push_back(std::make_pair(name, properties));

  // Any live threads will need to be updated to include the new module.
  auto it = threads_.begin();
  while (it != threads_.end()) {
    auto thread = it->lock();
    if (!thread) {
      // Thread has been destroyed; stop tracking it.
      it = threads_.erase(it);
    } else {
      bridge_->installPropertyModule(thread->state(), name, properties);
      it++;
    }
  }
}

}  // namespace lua
