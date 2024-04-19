/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_database.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "bridge.hpp"
#include "lua.hpp"

#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua_thread.hpp"
#include "lvgl.h"

#include "database.hpp"
#include "event_queue.hpp"
#include "index.hpp"
#include "property.hpp"
#include "records.hpp"
#include "service_locator.hpp"
#include "track.hpp"
#include "ui_events.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_db";

static constexpr char kDbIndexMetatable[] = "db_index";
static constexpr char kDbRecordMetatable[] = "db_record";
static constexpr char kDbIteratorMetatable[] = "db_iterator";

struct LuaIndexInfo {
  database::IndexId id;
  size_t name_size;
  char name_data[];

  auto name() -> std::string_view { return {name_data, name_size}; }
};

static_assert(std::is_trivially_destructible<LuaIndexInfo>());
static_assert(std::is_trivially_copy_assignable<LuaIndexInfo>());

static auto indexes(lua_State* state) -> int {
  Bridge* instance = Bridge::Get(state);

  lua_newtable(state);

  auto db = instance->services().database().lock();
  if (!db) {
    return 1;
  }

  for (const auto& i : db->getIndexes()) {
    LuaIndexInfo* data = reinterpret_cast<LuaIndexInfo*>(
        lua_newuserdata(state, sizeof(LuaIndexInfo) + i.name.size()));
    luaL_setmetatable(state, kDbIndexMetatable);
    *data = LuaIndexInfo{
        .id = i.id,
        .name_size = i.name.size(),
    };
    std::memcpy(data->name_data, i.name.data(), i.name.size());
    lua_rawseti(state, -2, i.id);
  }

  return 1;
}

static auto version(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto db = instance->services().database().lock();
  if (!db) {
    return 0;
  }
  auto res = db->schemaVersion();
  lua_pushlstring(L, res.data(), res.size());
  return 1;
}

static auto size(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto db = instance->services().database().lock();
  if (!db) {
    return 0;
  }
  lua_pushinteger(L, db->sizeOnDiskBytes());
  return 1;
}

static auto recreate(lua_State* L) -> int {
  ESP_LOGI(kTag, "recreate");
  return 0;
}

static auto update(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto db = instance->services().database().lock();
  if (!db) {
    return 0;
  }

  instance->services().bg_worker().Dispatch<void>(
      [=]() { db->updateIndexes(); });
  return 0;
}

static const struct luaL_Reg kDatabaseFuncs[] = {
    {"indexes", indexes},   {"version", version}, {"size", size},
    {"recreate", recreate}, {"update", update},   {NULL, NULL}};

/*
 * Struct to be used as userdata for the Lua representation of database records.
 * In order to push these large values into PSRAM as much as possible, memory
 * for these is allocated and managed by Lua. This struct must therefore be
 * trivially copyable.
 */
struct LuaRecord {
  std::variant<database::TrackId, database::IndexKey::Header> contents;
  size_t text_size;
  char text[];
};

static_assert(std::is_trivially_destructible<LuaRecord>());
static_assert(std::is_trivially_copy_assignable<LuaRecord>());

static auto push_lua_record(lua_State* L, const database::Record& r) -> void {
  // Create and init the userdata.
  LuaRecord* record = reinterpret_cast<LuaRecord*>(
      lua_newuserdata(L, sizeof(LuaRecord) + r.text().size()));
  luaL_setmetatable(L, kDbRecordMetatable);

  // Init all the fields
  *record = {
      .contents = r.contents(),
      .text_size = r.text().size(),
  };

  // Copy the string data across.
  std::memcpy(record->text, r.text().data(), r.text().size());
}

auto db_check_iterator(lua_State* L, int stack_pos) -> database::Iterator* {
  database::Iterator* it = *reinterpret_cast<database::Iterator**>(
      luaL_checkudata(L, stack_pos, kDbIteratorMetatable));
  return it;
}

static auto push_iterator(lua_State* state, const database::Iterator& it)
    -> void {
  database::Iterator** data = reinterpret_cast<database::Iterator**>(
      lua_newuserdata(state, sizeof(uintptr_t)));
  *data = new database::Iterator(it);
  luaL_setmetatable(state, kDbIteratorMetatable);
}

static auto db_iterate_prev(lua_State* state) -> int {
  database::Iterator* it = db_check_iterator(state, 1);
  std::optional<database::Record> res = (*it)--;

  if (res) {
    push_lua_record(state, *res);
  } else {
    lua_pushnil(state);
  }

  return 1;
}

static auto db_iterate(lua_State* state) -> int {
  database::Iterator* it = db_check_iterator(state, 1);
  std::optional<database::Record> res = (*it)++;

  if (res) {
    push_lua_record(state, *res);
  } else {
    lua_pushnil(state);
  }

  return 1;
}

static auto db_iterator_clone(lua_State* state) -> int {
  database::Iterator* it = db_check_iterator(state, 1);
  push_iterator(state, *it);
  return 1;
}

static auto db_iterator_gc(lua_State* state) -> int {
  database::Iterator* it = db_check_iterator(state, 1);
  delete it;
  return 0;
}

static const struct luaL_Reg kDbIteratorFuncs[] = {{"next", db_iterate},
                                                   {"prev", db_iterate_prev},
                                                   {"clone", db_iterator_clone},
                                                   {"__call", db_iterate},
                                                   {"__gc", db_iterator_gc},
                                                   {NULL, NULL}};

static auto record_text(lua_State* state) -> int {
  LuaRecord* data = reinterpret_cast<LuaRecord*>(
      luaL_checkudata(state, 1, kDbRecordMetatable));
  lua_pushlstring(state, data->text, data->text_size);
  return 1;
}

static auto record_contents(lua_State* state) -> int {
  LuaRecord* data = reinterpret_cast<LuaRecord*>(
      luaL_checkudata(state, 1, kDbRecordMetatable));

  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, database::TrackId>) {
          lua_pushinteger(state, arg);
        } else if constexpr (std::is_same_v<T, database::IndexKey::Header>) {
          Bridge* bridge = Bridge::Get(state);
          auto db = bridge->services().database().lock();
          if (!db) {
            lua_pushnil(state);
          } else {
            push_iterator(state, database::Iterator{db, arg});
          }
        }
      },
      data->contents);

  return 1;
}

static const struct luaL_Reg kDbRecordFuncs[] = {{"title", record_text},
                                                 {"contents", record_contents},
                                                 {"__tostring", record_text},
                                                 {NULL, NULL}};

static auto index_name(lua_State* state) -> int {
  LuaIndexInfo* data = reinterpret_cast<LuaIndexInfo*>(
      luaL_checkudata(state, 1, kDbIndexMetatable));
  if (data == NULL) {
    return 0;
  }
  lua_pushlstring(state, data->name_data, data->name_size);
  return 1;
}

static auto index_iter(lua_State* state) -> int {
  LuaIndexInfo* data = reinterpret_cast<LuaIndexInfo*>(
      luaL_checkudata(state, 1, kDbIndexMetatable));
  if (data == NULL) {
    return 0;
  }
  Bridge* bridge = Bridge::Get(state);
  auto db = bridge->services().database().lock();
  if (!db) {
    lua_pushnil(state);
  }
  push_iterator(state, database::Iterator{db, data->id});
  return 1;
}

static const struct luaL_Reg kDbIndexFuncs[] = {{"name", index_name},
                                                {"iter", index_iter},
                                                {"__tostring", index_name},
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
  lua_pushliteral(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);  // metatable.__index = metatable
  luaL_setfuncs(state, kDbIteratorFuncs, 0);

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
