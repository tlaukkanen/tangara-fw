/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/audio_events.hpp"
#include "lua/lua_database.hpp"

#include <memory>
#include <string>

#include "lua.hpp"

#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lvgl.h"

#include "audio/track_queue.hpp"
#include "database/database.hpp"
#include "database/index.hpp"
#include "database/track.hpp"
#include "events/event_queue.hpp"
#include "lua/bridge.hpp"
#include "lua/property.hpp"
#include "system_fsm/service_locator.hpp"
#include "ui/ui_events.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_queue";

static auto queue_add(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);

  if (lua_isinteger(state, 1)) {
    database::TrackId id = luaL_checkinteger(state, 1);
    instance->services().bg_worker().Dispatch<void>([=]() {
      audio::TrackQueue& queue = instance->services().track_queue();
      queue.append(id);
    });
  } else if (lua_isstring(state, 1)) {
    size_t len;
    const char* str = luaL_checklstring(state, 1, &len);
    std::string path{str, len};
    instance->services().bg_worker().Dispatch<void>([=]() {
      audio::TrackQueue& queue = instance->services().track_queue();
      queue.append(path);
    });
  } else {
    database::Iterator* it = db_check_iterator(state, 1);
    instance->services().bg_worker().Dispatch<void>([=]() {
      audio::TrackQueue& queue = instance->services().track_queue();
      queue.append(database::TrackIterator{*it});
    });
  }

  return 0;
}

static auto queue_clear(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);
  audio::TrackQueue& queue = instance->services().track_queue();
  queue.clear();
  return 0;
}

static auto queue_open_playlist(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);
  audio::TrackQueue& queue = instance->services().track_queue();
  size_t len = 0;
  const char* str = luaL_checklstring(state, 1, &len);
  if (!str) {
    return 0;
  }
  queue.clear();
  queue.openPlaylist(str);
  return 0;
}

static auto queue_play_from(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);
  audio::TrackQueue& queue = instance->services().track_queue();
  size_t len = 0;
  const char* str = luaL_checklstring(state, 1, &len);
  if (!str) {
    return 0;
  }
  auto pos = luaL_checkinteger(state, 2);
  queue.playFromPosition(str, pos);
  return 0;
}

static const struct luaL_Reg kQueueFuncs[] = {
    {"add", queue_add},
    {"clear", queue_clear},
    {"open_playlist", queue_open_playlist},
    {"play_from", queue_play_from},
    {NULL, NULL}};

static auto lua_queue(lua_State* state) -> int {
  luaL_newlib(state, kQueueFuncs);
  return 1;
}

auto RegisterQueueModule(lua_State* s) -> void {
  luaL_requiref(s, "queue", lua_queue, true);
  lua_pop(s, 1);
}

}  // namespace lua
