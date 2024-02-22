/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_thread.hpp"

#include <iostream>
#include <memory>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lua.hpp"

#include "bridge.hpp"
#include "event_queue.hpp"
#include "memory_resource.hpp"
#include "service_locator.hpp"
#include "ui_events.hpp"

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

auto LuaThread::Start(system_fsm::ServiceLocator& services) -> LuaThread* {
  auto alloc = std::make_unique<Allocator>();
  lua_State* state = lua_newstate(lua_alloc, alloc.get());
  if (!state) {
    return nullptr;
  }

  luaL_openlibs(state);
  lua_atpanic(state, lua_panic);

  return new LuaThread(alloc, state);
}

LuaThread::LuaThread(std::unique_ptr<Allocator>& alloc, lua_State* state)
    : alloc_(std::move(alloc)), state_(state) {}

LuaThread::~LuaThread() {
  lua_close(state_);
}

auto LuaThread::RunScript(const std::string& path) -> bool {
  int res = luaL_loadfilex(state_, path.c_str(), NULL);
  if (res != LUA_OK) {
    return false;
  }
  CallProtected(state_, 0, 0);
  return true;
}

auto LuaThread::RunString(const std::string& script) -> bool {
  int res = luaL_loadstring(state_, script.c_str());
  if (res != LUA_OK) {
    return false;
  }
  CallProtected(state_, 0, 0);
  return true;
}

auto LuaThread::DumpStack() -> void {
  int top = lua_gettop(state_);
  std::cout << "stack size: " << top << std::endl;
  for (size_t i = 1; i <= top; i++) {
    std::cout << "[" << i << "]\t" << luaL_typename(state_, i);
    switch (lua_type(state_, i)) {
      case LUA_TNUMBER:
        std::cout << "\t(";
        if (lua_isinteger(state_, i)) {
          std::cout << lua_tointeger(state_, i);
        } else {
          std::cout << lua_tonumber(state_, i);
        }
        std::cout << ")";
        break;
      case LUA_TSTRING:
        std::cout << "\t('" << lua_tostring(state_, i) << "')";
        break;
      case LUA_TBOOLEAN:
        std::cout << "\t(" << lua_toboolean(state_, i) << ")";
        break;
      case LUA_TNIL:
        // Value is implied.
        break;
      case LUA_TTABLE:
        lua_pushnil(state_);
        while (lua_next(state_, i) != 0) {
          // Keys
          std::cout << std::endl << "\t\t" << luaL_typename(state_, -2);
          if (lua_type(state_, -2) == LUA_TSTRING) {
            std::cout << "\t(" << lua_tostring(state_, -2) << ")";
          } else if (lua_type(state_, -2) == LUA_TNUMBER) {
            std::cout << "\t(" << lua_tonumber(state_, -2) << ")";
          }

          // Values
          std::cout << "\t\t" << luaL_typename(state_, -1);
          if (lua_type(state_, -1) == LUA_TSTRING) {
            std::cout << "\t(" << lua_tostring(state_, -1) << ")";
          } else if (lua_type(state_, -1) == LUA_TNUMBER) {
            std::cout << "\t(" << lua_tonumber(state_, -1) << ")";
          }
          // Pop the value; we don't care about it. Leave the key on the stack
          // for the next call to lua_next.
          lua_pop(state_, 1);
        }
        break;
      default:
        std::cout << "\t(" << lua_topointer(state_, i) << ")";
        break;
    }
    std::cout << std::endl;
  }
}

static int msg_handler(lua_State* L) {
  if (!lua_isstring(L, 1)) {
    return 1;
  }

  const char* msg = lua_tostring(L, 1);
  if (msg == NULL) {                         /* is error object not a string? */
    if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
      return 1;                              /* that is the message */
    else
      msg = lua_pushfstring(L, "(error object is a %s value)",
                            luaL_typename(L, 1));
  }

  /* append a standard traceback */
  luaL_traceback(L, L, msg, 1);
  return 1;
}

auto CallProtected(lua_State* s, int nargs, int nresults) -> int {
  int base = lua_gettop(s) - nargs;
  // Place our message handler under the function to be called.
  lua_pushcfunction(s, msg_handler);
  lua_insert(s, base);

  // Invoke the function.
  int ret = lua_pcall(s, nargs, nresults, base);
  if (ret != LUA_OK) {
    events::Ui().Dispatch(ui::OnLuaError{.message = lua_tostring(s, -1)});
  }

  // Clean up our message handler
  lua_remove(s, base);

  return ret;
}

}  // namespace lua
