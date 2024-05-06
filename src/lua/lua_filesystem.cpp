/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_filesystem.hpp"
#include <string>
#include <cstring>
#include "lauxlib.h"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_fs";

static constexpr char kFileEntryMetatable[] = "fs_file_entry";
static constexpr char kFileIteratorMetatable[] = "fs_iterator";

// Todo: Use std::pmr::string for paths/dirs
struct LuaFileEntry {
  bool isHidden;
  bool isDirectory;
  bool isTrack;
  size_t path_size;
  char path[];
};

static_assert(std::is_trivially_destructible<LuaFileEntry>());
static_assert(std::is_trivially_copy_assignable<LuaFileEntry>());

static auto push_lua_file_entry(lua_State* L, const database::FileEntry& r) -> void {
  // Create and init the userdata.
  LuaFileEntry* file_entry = reinterpret_cast<LuaFileEntry*>(
      lua_newuserdata(L, sizeof(LuaFileEntry) + r.filepath.size()));
  luaL_setmetatable(L, kFileEntryMetatable);

  // Init all the fields
  *file_entry = {
      .isHidden = r.isHidden,
      .isDirectory = r.isDirectory,
      .isTrack = r.isTrack,
      .path_size = r.filepath.size(),
  };

  // Copy the string data across.
  std::memcpy(file_entry->path, r.filepath.data(), r.filepath.size());
}

auto check_file_iterator(lua_State* L, int stack_pos) -> database::FileIterator* {
  database::FileIterator* it = *reinterpret_cast<database::FileIterator**>(
      luaL_checkudata(L, stack_pos, kFileIteratorMetatable));
  return it;
}

static auto push_iterator(lua_State* state, const database::FileIterator& it)
    -> void {
  database::FileIterator** data = reinterpret_cast<database::FileIterator**>(
      lua_newuserdata(state, sizeof(uintptr_t)));
  *data = new database::FileIterator(it); // TODO...
  luaL_setmetatable(state, kFileIteratorMetatable);
}

static auto fs_iterate_prev(lua_State* state) -> int {
  database::FileIterator* it = check_file_iterator(state, 1);
  it->prev();
  std::optional<database::FileEntry> res = it->value();

  if (res) {
    push_lua_file_entry(state, *res);
  } else {
    lua_pushnil(state);
  }

  return 1;
}

static auto fs_iterate(lua_State* state) -> int {
  database::FileIterator* it = check_file_iterator(state, 1);
  it->next();
  std::optional<database::FileEntry> res = it->value();

  if (res) {
    push_lua_file_entry(state, *res);
  } else {
    lua_pushnil(state);
  }

  return 1;
}

static auto fs_iterator_clone(lua_State* state) -> int {
  database::FileIterator* it = check_file_iterator(state, 1);
  push_iterator(state, *it);
  return 1;
}

static auto fs_iterator_gc(lua_State* state) -> int {
  database::FileIterator* it = check_file_iterator(state, 1);
  delete it;
  return 0;
}

static const struct luaL_Reg kFileIteratorFuncs[] = {{"next", fs_iterate},
                                                   {"prev", fs_iterate_prev},
                                                   {"clone", fs_iterator_clone},
                                                   {"__call", fs_iterate},
                                                   {"__gc", fs_iterator_gc},
                                                   {NULL, NULL}};

static auto file_entry_path(lua_State* state) -> int {
  LuaFileEntry* data = reinterpret_cast<LuaFileEntry*>(
      luaL_checkudata(state, 1, kFileEntryMetatable));
  lua_pushlstring(state, data->path, data->path_size);
  return 1;
}

static auto file_entry_is_dir(lua_State* state) -> int {
  LuaFileEntry* data = reinterpret_cast<LuaFileEntry*>(
      luaL_checkudata(state, 1, kFileEntryMetatable));
  lua_pushboolean(state, data->isDirectory);
  return 1;
}

static auto file_entry_is_hidden(lua_State* state) -> int {
  LuaFileEntry* data = reinterpret_cast<LuaFileEntry*>(
      luaL_checkudata(state, 1, kFileEntryMetatable));
  lua_pushboolean(state, data->isHidden);
  return 1;
}

static auto file_entry_is_track(lua_State* state) -> int {
  LuaFileEntry* data = reinterpret_cast<LuaFileEntry*>(
      luaL_checkudata(state, 1, kFileEntryMetatable));
  lua_pushboolean(state, data->isTrack);
  return 1;
}

static const struct luaL_Reg kFileEntryFuncs[] = {{"filepath", file_entry_path},
                                                 {"is_directory", file_entry_is_dir},
                                                 {"is_hidden", file_entry_is_hidden},
                                                 {"is_track", file_entry_is_track},
                                                 {"__tostring", file_entry_path},
                                                 {NULL, NULL}};

static auto fs_new_iterator(lua_State* state) -> int {
  // Takes a filepath as a string and returns a new FileIterator
  // on that directory
  std::string filepath = luaL_checkstring(state, -1);
  database::FileIterator iter(filepath);
   push_iterator(state, iter);
  return 1;
}

static const struct luaL_Reg kDatabaseFuncs[] = {{"iterator", fs_new_iterator},
                                                 {NULL, NULL}};

static auto lua_filesystem(lua_State* state) -> int {
  luaL_newmetatable(state, kFileIteratorMetatable);
  lua_pushliteral(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);  // metatable.__index = metatable
  luaL_setfuncs(state, kFileIteratorFuncs, 0);

  luaL_newmetatable(state, kFileEntryMetatable);
  lua_pushliteral(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);  // metatable.__index = metatable
  luaL_setfuncs(state, kFileEntryFuncs, 0);

  luaL_newlib(state, kDatabaseFuncs);
  return 1;
}

auto RegisterFileSystemModule(lua_State* s) -> void {
  luaL_requiref(s, "filesystem", lua_filesystem, true);
  lua_pop(s, 1);
}

}  // namespace lua
