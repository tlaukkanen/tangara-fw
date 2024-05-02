
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

static auto set_style(lua_State* L) -> int {
  // Get the object and class name from the stack
  std::string class_name = luaL_checkstring(L, -1);
  lv_obj_t* obj = luavgl_to_obj(L, -2);
  if (obj != NULL) {
    ui::themes::Theme::instance()->ApplyStyle(obj, class_name);
  }
  return 0;
}

static auto set_theme(lua_State* L) -> int {
  std::string class_name;
  luaL_checktype(L, -1, LUA_TTABLE);
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
              ESP_LOGI("lua_theme", "Style was null or malformed");
              return 0;
            } else {
              ui::themes::Theme::instance()->AddStyle(class_name, selector, style);
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

static const struct luaL_Reg kThemeFuncs[] = {{"set", set_theme}, {"set_style", set_style}, {NULL, NULL}};

static auto lua_theme(lua_State* L) -> int {
  luaL_newlib(L, kThemeFuncs);
  return 1;
}

auto RegisterThemeModule(lua_State* L) -> void {
  luaL_requiref(L, "theme", lua_theme, true);
  lua_pop(L, 1);
}

}  // namespace lua
