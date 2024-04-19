
/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_version.hpp"

#include <string>

#include "bridge.hpp"
#include "lua.hpp"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua_thread.hpp"

namespace lua {

static auto esp(lua_State* L) -> int {
  auto desc = esp_app_get_description();
  lua_pushstring(L, desc->version);
  return 1;
}

static auto samd(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto& samd = instance->services().samd();
  auto version = samd.Version();
  lua_pushlstring(L, version.data(), version.size());
  return 1;
}

static auto update_samd(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto& samd = instance->services().samd();
  samd.ResetToFlashSamd();
  return 0;
}

static auto collator(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto& collator = instance->services().collator();
  auto version = collator.Describe().value_or("None");
  lua_pushlstring(L, version.data(), version.size());
  return 1;
}

static const struct luaL_Reg kVersionFuncs[] = {{"esp", esp},
                                                {"samd", samd},
                                                {"collator", collator},
                                                {"update_samd", update_samd},
                                                {NULL, NULL}};

static auto lua_version(lua_State* L) -> int {
  luaL_newlib(L, kVersionFuncs);
  return 1;
}

auto RegisterVersionModule(lua_State* L) -> void {
  luaL_requiref(L, "version", lua_version, true);
  lua_pop(L, 1);
}

}  // namespace lua
