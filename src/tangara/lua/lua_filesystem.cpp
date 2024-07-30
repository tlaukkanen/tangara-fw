/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua/lua_filesystem.hpp"
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
  std::string path;
  std::string name;
};

static auto push_lua_file_entry(lua_State* L, const lua::FileEntry& r) -> void {
  lua::FileEntry** entry = reinterpret_cast<lua::FileEntry**>(
      lua_newuserdata(L, sizeof(uintptr_t)));
  *entry = new lua::FileEntry(r);
  luaL_setmetatable(L, kFileEntryMetatable);
}

auto check_file_entry(lua_State* L, int stack_pos) -> lua::FileEntry* {
  lua::FileEntry* entry = *reinterpret_cast<lua::FileEntry**>(
      luaL_checkudata(L, stack_pos, kFileEntryMetatable));
  return entry;
}

auto check_file_iterator(lua_State* L, int stack_pos) -> lua::FileIterator* {
  lua::FileIterator* it = *reinterpret_cast<lua::FileIterator**>(
      luaL_checkudata(L, stack_pos, kFileIteratorMetatable));
  return it;
}

static auto push_iterator(lua_State* state, const lua::FileIterator& it)
    -> void {
  lua::FileIterator** data = reinterpret_cast<lua::FileIterator**>(
      lua_newuserdata(state, sizeof(uintptr_t)));
  *data = new lua::FileIterator(it);
  luaL_setmetatable(state, kFileIteratorMetatable);
}

static auto fs_iterate_prev(lua_State* state) -> int {
  lua::FileIterator* it = check_file_iterator(state, 1);
  it->prev();
  std::optional<lua::FileEntry> res = it->value();

  if (res) {
    push_lua_file_entry(state, *res);
  } else {
    lua_pushnil(state);
  }

  return 1;
}

static auto fs_iterate(lua_State* state) -> int {
  lua::FileIterator* it = check_file_iterator(state, 1);
  it->next();
  std::optional<lua::FileEntry> res = it->value();

  if (res) {
    push_lua_file_entry(state, *res);
  } else {
    lua_pushnil(state);
  }

  return 1;
}

static auto fs_iterator_clone(lua_State* state) -> int {
  lua::FileIterator* it = check_file_iterator(state, 1);
  push_iterator(state, *it);
  return 1;
}

static auto fs_iterator_gc(lua_State* state) -> int {
  lua::FileIterator* it = check_file_iterator(state, 1);
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
  lua::FileEntry* entry = check_file_entry(state, 1);
  lua_pushlstring(state, entry->filepath.c_str(), entry->filepath.size());
  return 1;
}

static auto file_entry_is_dir(lua_State* state) -> int {
  lua::FileEntry* entry = check_file_entry(state, 1);
  lua_pushboolean(state, entry->isDirectory);
  return 1;
}

static auto file_entry_is_hidden(lua_State* state) -> int {
  lua::FileEntry* entry = check_file_entry(state, 1);
  lua_pushboolean(state, entry->isHidden);
  return 1;
}

static auto file_entry_is_track(lua_State* state) -> int {
  lua::FileEntry* entry = check_file_entry(state, 1);
  lua_pushboolean(state, entry->isTrack);
  return 1;
}

static auto file_entry_name(lua_State* state) -> int {
  lua::FileEntry* entry = check_file_entry(state, 1);
  lua_pushlstring(state, entry->name.c_str(), entry->name.size());
  return 1;
}

static auto file_entry_gc(lua_State* state) -> int {
  lua::FileEntry* entry = check_file_entry(state, 1);
  delete entry;
  return 1;
}

static const struct luaL_Reg kFileEntryFuncs[] = {{"filepath", file_entry_path},
                                                 {"name", file_entry_name},
                                                 {"is_directory", file_entry_is_dir},
                                                 {"is_hidden", file_entry_is_hidden},
                                                 {"is_track", file_entry_is_track},
                                                 {"__tostring", file_entry_name},
                                                   {"__gc", file_entry_gc},
                                                 {NULL, NULL}};

static auto fs_new_iterator(lua_State* state) -> int {
  // Takes a filepath as a string and returns a new FileIterator
  // on that directory
  std::string filepath = luaL_checkstring(state, -1);
  lua::FileIterator iter(filepath, false);
   push_iterator(state, iter);
  return 1;
}

static const struct luaL_Reg kFilesystemFuncs[] = {{"iterator", fs_new_iterator},
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

  luaL_newlib(state, kFilesystemFuncs);
  return 1;
}

auto RegisterFileSystemModule(lua_State* s) -> void {
  luaL_requiref(s, "filesystem", lua_filesystem, true);
  lua_pop(s, 1);
}

}  // namespace lua
