
/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
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
#include "luavgl.h"
#include "themes.hpp"

namespace lua {

static auto set_theme(lua_State* L) -> int {
  // lv_style_t* style = luavgl_to_style(L, -1);
  // if (style == NULL) {
  //   ESP_LOGI("DANIEL", "Style was null or malformed??");
  //   return 0;
  // }

  // ESP_LOGI("DANIEL", "GOT ONE!");
  // themes::Theme::instance()->...;

  /* table is in the stack at index 't' */
  std::string class_name;
  lua_pushnil(L);  /* first key */
  while (lua_next(L, -2) != 0) {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (lua_type(L, -2) == LUA_TSTRING) {
      class_name = lua_tostring(L, -2);
    }
    if (lua_type(L, -1) == LUA_TTABLE) {
      // Nesting
      lua_pushnil(L); // First key
      while (lua_next(L, -2) != 0) {
        // Nesting the second
        int selector = -1;
        lv_style_t* style = NULL;
        lua_pushnil(L); // First key
        while (lua_next(L, -2) != 0) {
          int idx = lua_tointeger(L, -2);
          if (idx == 1) {
            // Selector
            selector = lua_tointeger(L, -1);
          } else if (idx == 2) {
            // Style
            lv_style_t* style = luavgl_to_style(L, -1);
            if (style == NULL) {
              ESP_LOGI("DANIEL", "Style was null or malformed??");
              return 0;
            } else {
              ui::themes::Theme::instance()->AddStyle(class_name, selector, style);
              ESP_LOGI("DANIEL", "Got style for class %s with selector %d", class_name.c_str(), selector);
            }
          }
          lua_pop(L, 1); 
        }
        lua_pop(L, 1); 
      }
    }
    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
  return 0;
}

static const struct luaL_Reg kThemeFuncs[] = {{"set", set_theme}, {NULL, NULL}};

static auto lua_theme(lua_State* L) -> int {
  luaL_newlib(L, kThemeFuncs);
  return 1;
}

auto RegisterThemeModule(lua_State* L) -> void {
  luaL_requiref(L, "theme", lua_theme, true);
  lua_pop(L, 1);
}

}  // namespace lua
