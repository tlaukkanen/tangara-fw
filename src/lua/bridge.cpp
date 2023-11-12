/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bridge.hpp"

#include <memory>
#include <string>

#include "esp_log.h"
#include "event_queue.hpp"
#include "lua.h"
#include "lua.hpp"
#include "lvgl.h"
#include "service_locator.hpp"
#include "ui_events.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_bridge";
static constexpr char kBridgeKey[] = "bridge";

static auto open_settings_fn(lua_State* state) -> int {
  events::Ui().Dispatch(ui::internal::ShowSettingsPage{
      .page = ui::internal::ShowSettingsPage::Page::kRoot});
  return 0;
}

static auto open_now_playing_fn(lua_State* state) -> int {
  events::Ui().Dispatch(ui::internal::ShowNowPlaying{});
  return 0;
}

static auto open_browse_fn(lua_State* state) -> int {
  int index = luaL_checkinteger(state, 1);
  events::Ui().Dispatch(ui::internal::IndexSelected{
      .id = static_cast<uint8_t>(index),
  });
  return 0;
}

static const struct luaL_Reg kLegacyUiFuncs[] = {
    {"open_settings", open_settings_fn},
    {"open_now_playing", open_now_playing_fn},
    {"open_browse", open_browse_fn},
    {NULL, NULL}};

static auto lua_legacy_ui(lua_State* state) -> int {
  luaL_newlib(state, kLegacyUiFuncs);
  return 1;
}

static auto get_indexes(lua_State* state) -> int {
  lua_pushstring(state, kBridgeKey);
  lua_gettable(state, LUA_REGISTRYINDEX);
  Bridge* instance = reinterpret_cast<Bridge*>(lua_touserdata(state, -1));

  lua_newtable(state);

  auto db = instance->services().database().lock();
  if (!db) {
    return 1;
  }

  for (const auto& i : db->GetIndexes()) {
    lua_pushstring(state, i.name.c_str());
    lua_rawseti(state, -2, i.id);
  }

  return 1;
}

static const struct luaL_Reg kDatabaseFuncs[] = {{"get_indexes", get_indexes},
                                                 {NULL, NULL}};

static auto lua_database(lua_State* state) -> int {
  luaL_newlib(state, kDatabaseFuncs);
  return 1;
}

Bridge::Bridge(system_fsm::ServiceLocator& services, lua_State& s)
    : services_(services), state_(s) {
  lua_pushstring(&s, kBridgeKey);
  lua_pushlightuserdata(&s, this);
  lua_settable(&s, LUA_REGISTRYINDEX);

  luaL_requiref(&s, "legacy_ui", lua_legacy_ui, true);
  lua_pop(&s, 1);

  luaL_requiref(&s, "database", lua_database, true);
  lua_pop(&s, 1);
}

}  // namespace lua
