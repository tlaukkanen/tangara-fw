/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_thread.hpp"

#include <memory>

#include "lua.hpp"

#include "font/lv_font_loader.h"
#include "luavgl.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "service_locator.hpp"
#include "ui_events.hpp"

LV_FONT_DECLARE(font_fusion_12);
LV_FONT_DECLARE(font_fusion_10);

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

static auto make_font_cb(const char* name, int size, int weight)
    -> const lv_font_t* {
  if (std::string{"fusion"} == name) {
    if (size == 12) {
      return &font_fusion_12;
    }
    if (size == 10) {
      return &font_fusion_10;
    }
  }
  return NULL;
}

static auto delete_font_cb(lv_font_t* font) -> void {}

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
    luavgl_set_pcall(state, CallProtected);
    luavgl_set_font_extension(state, make_font_cb, delete_font_cb);
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
