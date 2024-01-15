#include "lua.h"
#include "luavgl.h"
#include "private.h"
#include <src/widgets/lv_slider.h>

static int luavgl_slider_create(lua_State *L) {
  return luavgl_obj_create_helper(L, lv_slider_create);
}

static void _lv_slider_set_range(void *obj, lua_State *L) {
  int min = 0, max = 100;

  int type = lua_type(L, -1);
  if (type == LUA_TTABLE) {
    lua_getfield(L, -1, "min");
    min = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "max");
    max = luavgl_tointeger(L, -1);
    lua_pop(L, 1);
  }

  lv_slider_set_range(obj, min, max);
}

static void _lv_slider_set_value(void *obj, int value) {
  lv_slider_set_value(obj, value, LV_ANIM_OFF);
}

static const luavgl_value_setter_t slider_property_table[] = {
    {"range", SETTER_TYPE_STACK, {.setter_stack = _lv_slider_set_range}},
    {"value", SETTER_TYPE_INT, {.setter = (setter_int_t)_lv_slider_set_value}},
};

LUALIB_API int luavgl_slider_set_property_kv(lua_State *L, void *data) {
  lv_obj_t *obj = data;
  int ret = luavgl_set_property(L, obj, slider_property_table);

  if (ret == 0) {
    return 0;
  }
  /* a base obj property? */
  ret = luavgl_obj_set_property_kv(L, obj);
  if (ret != 0) {
    debug("unkown property for slider.\n");
  }

  return ret;
}

static int luavgl_slider_set(lua_State *L) {
  lv_obj_t *obj = luavgl_to_obj(L, 1);

  if (!lua_istable(L, -1)) {
    luaL_error(L, "expect a table on 2nd para.");
    return 0;
  }

  luavgl_iterate(L, -1, luavgl_slider_set_property_kv, obj);

  return 0;
}

static int luavgl_slider_value(lua_State *L) {
  lv_obj_t *obj = luavgl_to_obj(L, 1);
  lua_pushinteger(L, lv_slider_get_value(obj));
  return 1;
}

static int luavgl_slider_tostring(lua_State *L) {
  lv_obj_t *obj = luavgl_to_obj(L, 1);
  lua_pushfstring(L, "lv_slider:%p", obj);
  return 1;
}

static const luaL_Reg luavgl_slider_methods[] = {
    {"set", luavgl_slider_set},
    {"value", luavgl_slider_value},
    {NULL, NULL},
};

static void luavgl_slider_init(lua_State *L) {
  luavgl_obj_newmetatable(L, &lv_slider_class, "lv_slider",
                          luavgl_slider_methods);
  lua_pushcfunction(L, luavgl_slider_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_pop(L, 1);
}
