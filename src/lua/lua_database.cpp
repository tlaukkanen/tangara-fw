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
#include "ui_events.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_db";

static constexpr char kDbIndexMetatable[] = "db_index";
static constexpr char kDbRecordMetatable[] = "db_record";
static constexpr char kDbIteratorMetatable[] = "db_record";

static auto indexes(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);

  lua_newtable(state);

  auto db = instance->services().database().lock();
  if (!db) {
    return 1;
  }

  for (const auto& i : db->GetIndexes()) {
    database::IndexInfo** data = reinterpret_cast<database::IndexInfo**>(
        lua_newuserdata(state, sizeof(uintptr_t)));
    luaL_setmetatable(state, kDbIndexMetatable);
    *data = new database::IndexInfo{i};
    lua_rawseti(state, -2, i.id);
  }

  return 1;
}

static const struct luaL_Reg kDatabaseFuncs[] = {{"indexes", indexes},
                                                 {NULL, NULL}};

static auto db_iterate(lua_State* state) -> int {
  luaL_checktype(state, 1, LUA_TFUNCTION);
  int callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  database::Iterator* it = *reinterpret_cast<database::Iterator**>(
      lua_touserdata(state, lua_upvalueindex(1)));

  it->Next([=](std::optional<database::IndexRecord> res) {
    events::Ui().RunOnTask([=]() {
      lua_rawgeti(state, LUA_REGISTRYINDEX, callback_ref);
      if (res) {
        database::IndexRecord** record =
            reinterpret_cast<database::IndexRecord**>(
                lua_newuserdata(state, sizeof(uintptr_t)));
        *record = new database::IndexRecord(*res);
        luaL_setmetatable(state, kDbRecordMetatable);
      } else {
        lua_pushnil(state);
      }
      lua_call(state, 1, 0);
      luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
    });
  });
  return 0;
}

static auto db_iterator_gc(lua_State* state) -> int {
  database::Iterator** it = reinterpret_cast<database::Iterator**>(
      luaL_checkudata(state, 1, kDbIteratorMetatable));
  if (it != NULL) {
    delete *it;
  }
  return 0;
}

static auto push_iterator(
    lua_State* state,
    std::variant<database::Continuation, database::IndexInfo> val) -> void {
  Bridge* instance = Bridge::Get(state);
  database::Iterator** data = reinterpret_cast<database::Iterator**>(
      lua_newuserdata(state, sizeof(uintptr_t)));
  std::visit(
      [&](auto&& arg) {
        *data = new database::Iterator(instance->services().database(), arg);
      },
      val);
  luaL_setmetatable(state, kDbIteratorMetatable);
  lua_pushcclosure(state, db_iterate, 1);
}

static auto record_text(lua_State* state) -> int {
  database::IndexRecord* data = *reinterpret_cast<database::IndexRecord**>(
      luaL_checkudata(state, 1, kDbRecordMetatable));
  lua_pushstring(state,
                 data->text().value_or("[tell jacqueline u saw this]").c_str());
  return 1;
}

static auto record_contents(lua_State* state) -> int {
  database::IndexRecord* data = *reinterpret_cast<database::IndexRecord**>(
      luaL_checkudata(state, 1, kDbRecordMetatable));

  if (data->track()) {
    lua_pushinteger(state, *data->track());
  } else {
    push_iterator(state, data->Expand(1).value());
  }

  return 1;
}

static auto record_gc(lua_State* state) -> int {
  database::IndexRecord** data = reinterpret_cast<database::IndexRecord**>(
      luaL_checkudata(state, 1, kDbRecordMetatable));
  if (data != NULL) {
    delete *data;
  }
  return 0;
}

static const struct luaL_Reg kDbRecordFuncs[] = {{"title", record_text},
                                                 {"contents", record_contents},
                                                 {"__tostring", record_text},
                                                 {"__gc", record_gc},
                                                 {NULL, NULL}};

static auto index_name(lua_State* state) -> int {
  database::IndexInfo** data = reinterpret_cast<database::IndexInfo**>(
      luaL_checkudata(state, 1, kDbIndexMetatable));
  if (data == NULL) {
    return 0;
  }
  lua_pushstring(state, (*data)->name.c_str());
  return 1;
}

static auto index_iter(lua_State* state) -> int {
  database::IndexInfo** data = reinterpret_cast<database::IndexInfo**>(
      luaL_checkudata(state, 1, kDbIndexMetatable));
  if (data == NULL) {
    return 0;
  }
  push_iterator(state, **data);
  return 1;
}

static auto index_gc(lua_State* state) -> int {
  database::IndexInfo** data = reinterpret_cast<database::IndexInfo**>(
      luaL_checkudata(state, 1, kDbIndexMetatable));
  if (data != NULL) {
    delete *data;
  }
  return 0;
}

static const struct luaL_Reg kDbIndexFuncs[] = {{"name", index_name},
                                                {"iter", index_iter},
                                                {"__tostring", index_name},
                                                {"__gc", index_gc},
                                                {NULL, NULL}};

static auto lua_database(lua_State* state) -> int {
  // Metatable for indexes
  luaL_newmetatable(state, kDbIndexMetatable);

  lua_pushliteral(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);  // metatable.__index = metatable

  // Add member funcs to the metatable.
  luaL_setfuncs(state, kDbIndexFuncs, 0);

  luaL_newmetatable(state, kDbIteratorMetatable);
  lua_pushliteral(state, "__gc");
  lua_pushcfunction(state, db_iterator_gc);
  lua_settable(state, -3);

  luaL_newmetatable(state, kDbRecordMetatable);
  lua_pushliteral(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);  // metatable.__index = metatable
  luaL_setfuncs(state, kDbRecordFuncs, 0);

  luaL_newlib(state, kDatabaseFuncs);
  return 1;
}

auto RegisterDatabaseModule(lua_State* s) -> void {
  luaL_requiref(s, "database", lua_database, true);
  lua_pop(s, 1);
}

}  // namespace lua