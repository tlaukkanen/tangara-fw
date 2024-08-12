
/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua/lua_version.hpp"

#include <string>

#include "lua.hpp"
#include "lua/bridge.hpp"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua/lua_thread.hpp"
#include "luavgl.h"
#include "ui/themes.hpp"

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
  lua_pushnil(L); /* first key */
  while (lua_next(L, -2) != 0) {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (lua_type(L, -2) == LUA_TSTRING) {
      class_name = lua_tostring(L, -2);
    }
    if (lua_type(L, -1) == LUA_TTABLE) {
      // Nesting
      lua_pushnil(L);  // First key
      while (lua_next(L, -2) != 0) {
        // Nesting the second
        int selector = -1;
        lua_pushnil(L);  // First key
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
              ui::themes::Theme::instance()->AddStyle(class_name, selector,
                                                      style);
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


static auto load_theme(lua_State* L) -> int {
  std::string filename = luaL_checkstring(L, -1);
  // Set the theme filename in non-volatile storage
  Bridge* instance = Bridge::Get(L);
  // Load the theme using lua
  auto status = luaL_loadfile(L, filename.c_str());
  if (status != LUA_OK) {
    lua_pushboolean(L, false);
    return 1;
  }
  status = lua::CallProtected(L, 0, 1);
  if (status == LUA_OK) {
    ui::themes::Theme::instance()->Reset();
    set_theme(L);
    instance->services().nvs().InterfaceTheme(filename);
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }

  return 1;
}

static auto theme_filename(lua_State* L) -> int {
  Bridge* instance = Bridge::Get(L);
  auto file = instance->services().nvs().InterfaceTheme().value_or("/lua/theme_light.lua");
  lua_pushstring(L, file.c_str());
  return 1;
}

static const struct luaL_Reg kThemeFuncs[] = {{"set", set_theme},
                                              {"set_style", set_style},
                                              {"load_theme", load_theme},
                                              {"theme_filename", theme_filename},
                                              {NULL, NULL}};

static auto lua_theme(lua_State* L) -> int {
  luaL_newlib(L, kThemeFuncs);
  return 1;
}

auto RegisterThemeModule(lua_State* L) -> void {
  luaL_requiref(L, "theme", lua_theme, true);
  lua_pop(L, 1);
}

}  // namespace lua
