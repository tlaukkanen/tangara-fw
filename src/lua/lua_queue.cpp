/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_database.hpp"

#include <memory>
#include <string>

#include "lua.hpp"

#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lvgl.h"

#include "database.hpp"
#include "event_queue.hpp"
#include "index.hpp"
#include "property.hpp"
#include "service_locator.hpp"
#include "source.hpp"
#include "ui_events.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_queue";

static auto queue_add(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);

  if (lua_isinteger(state, 1)) {
    instance->services().track_queue().AddLast(luaL_checkinteger(state, 1));
  } else {
    database::Iterator* it = db_check_iterator(state, 1);
    instance->services().track_queue().IncludeLast(
        std::make_shared<playlist::IteratorSource>(*it));
  }

  return 0;
}

static auto queue_clear(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);
  instance->services().track_queue().Clear();
  return 0;
}

static const struct luaL_Reg kQueueFuncs[] = {{"add", queue_add},
                                              {"clear", queue_clear},
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