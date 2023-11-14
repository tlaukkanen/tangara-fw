/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_thread.hpp"

#include <memory>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lua.hpp"
#include "luavgl.h"
#include "service_locator.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua";

class Allocator {
 public:
  Allocator() : total_allocated_(0) {}

  auto alloc(void* ptr, size_t osize, size_t nsize) -> void* {
    total_allocated_ = total_allocated_ - osize + nsize;
    // ESP_LOGI(kTag, "lua realloc -> %u KiB", total_allocated_ / 1024);
    if (nsize == 0) {
      heap_caps_free(ptr);
      return NULL;
    } else {
      return heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM);
    }
  }

 private:
  size_t total_allocated_;
};

static auto lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
    -> void* {
  Allocator* instance = reinterpret_cast<Allocator*>(ud);
  return instance->alloc(ptr, osize, nsize);
}

static int lua_panic(lua_State* L) {
  ESP_LOGE(kTag, "!! PANIC !! %s", lua_tostring(L, -1));
  return 0;
}

auto LuaThread::Start(system_fsm::ServiceLocator& services, lv_obj_t* lvgl_root)
    -> LuaThread* {
  auto alloc = std::make_unique<Allocator>();
  lua_State* state = lua_newstate(lua_alloc, alloc.get());
  if (!state) {
    return nullptr;
  }

  luaL_openlibs(state);
  lua_atpanic(state, lua_panic);

  auto bridge = std::make_unique<Bridge>(services, *state);

  // FIXME: luavgl init should probably be a part of the bridge.
  if (lvgl_root) {
    luavgl_set_root(state, lvgl_root);
    luaL_requiref(state, "lvgl", luaopen_lvgl, true);
    lua_pop(state, 1);
  }

  return new LuaThread(alloc, bridge, state);
}

LuaThread::LuaThread(std::unique_ptr<Allocator>& alloc,
                     std::unique_ptr<Bridge>& bridge,
                     lua_State* state)
    : alloc_(std::move(alloc)), bridge_(std::move(bridge)), state_(state) {}

LuaThread::~LuaThread() {
  lua_close(state_);
}

auto LuaThread::RunScript(const std::string& path) -> bool {
  int res = luaL_loadfilex(state_, path.c_str(), NULL);
  if (res != LUA_OK) {
    return false;
  }
  res = lua_pcall(state_, 0, 0, 0);
  if (res) {
    const char* msg = lua_tostring(state_, -1);
    lua_writestring(msg, strlen(msg));
    lua_writeline();
    lua_pop(state_, 1);
  }
  return true;
}

}  // namespace lua
