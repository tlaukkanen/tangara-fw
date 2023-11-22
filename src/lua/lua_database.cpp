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
#include "records.hpp"
#include "service_locator.hpp"
#include "track.hpp"
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

/*
 * Struct to be used as userdata for the Lua representation of database records.
 * In order to push these large values into PSRAM as much as possible, memory
 * for these is allocated and managed by Lua. This struct must therefore be
 * trivially copyable.
 */
struct LuaRecord {
  database::TrackId id_or_zero;
  database::IndexKey::Header header_at_next_depth;
  size_t text_size;
  char text[];
};

static_assert(std::is_trivially_copyable_v<LuaRecord> == true);

static auto push_lua_record(lua_State* L, const database::IndexRecord& r)
    -> void {
  // Bake out the text into something concrete.
  auto text = r.text().value_or("");

  // Create and init the userdata.
  LuaRecord* record = reinterpret_cast<LuaRecord*>(
      lua_newuserdata(L, sizeof(LuaRecord) + text.size()));
  luaL_setmetatable(L, kDbRecordMetatable);

  // Init all the fields
  *record = {
      .id_or_zero = r.track().value_or(0),
      .header_at_next_depth = r.ExpandHeader(),
      .text_size = text.size(),
  };

  // Copy the string data across.
  std::memcpy(record->text, text.data(), text.size());
}

static auto db_iterate(lua_State* state) -> int {
  luaL_checktype(state, 1, LUA_TFUNCTION);
  int callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  database::Iterator* it = *reinterpret_cast<database::Iterator**>(
      lua_touserdata(state, lua_upvalueindex(1)));

  it->Next([=](std::optional<database::IndexRecord> res) {
    events::Ui().RunOnTask([=]() {
      lua_rawgeti(state, LUA_REGISTRYINDEX, callback_ref);
      if (res) {
        push_lua_record(state, *res);
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
  LuaRecord* data = reinterpret_cast<LuaRecord*>(
      luaL_checkudata(state, 1, kDbRecordMetatable));
  lua_pushlstring(state, data->text, data->text_size);
  return 1;
}

static auto record_contents(lua_State* state) -> int {
  LuaRecord* data = reinterpret_cast<LuaRecord*>(
      luaL_checkudata(state, 1, kDbRecordMetatable));

  if (data->id_or_zero) {
    lua_pushinteger(state, data->id_or_zero);
  } else {
    std::string p = database::EncodeIndexPrefix(data->header_at_next_depth);
    push_iterator(state, database::Continuation{
                             .prefix = {p.data(), p.size()},
                             .start_key = {p.data(), p.size()},
                             .forward = true,
                             .was_prev_forward = true,
                             .page_size = 1,
                         });
  }

  return 1;
}

static const struct luaL_Reg kDbRecordFuncs[] = {{"title", record_text},
                                                 {"contents", record_contents},
                                                 {"__tostring", record_text},
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
