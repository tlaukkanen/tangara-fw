#include "luavgl.h"
#include "private.h"
#include <src/misc/lv_anim.h>
#include <src/widgets/lv_bar.h>

static int luavgl_bar_create(lua_State *L)
{
  return luavgl_obj_create_helper(L, lv_bar_create);
}

static void _lv_bar_set_range(void *obj, lua_State *L)
{
  int min=0, max=100;

  int type = lua_type(L, -1);
  if (type == LUA_TTABLE) {
    lua_getfield(L, -1, "min");
    min = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "max");
    max = luavgl_tointeger(L, -1);
    lua_pop(L, 1);
  }
  
  lv_bar_set_range(obj, min, max);
}

static void _lv_bar_set_value(void *obj, int value)
{
  lv_bar_set_value(obj, value, LV_ANIM_OFF);
}

static const luavgl_value_setter_t bar_property_table[] = {
    {"range", SETTER_TYPE_STACK, {.setter_stack = _lv_bar_set_range}},
    {"value", SETTER_TYPE_INT, {.setter = (setter_int_t)_lv_bar_set_value}},
};

LUALIB_API int luavgl_bar_set_property_kv(lua_State *L, void *data)
{
  lv_obj_t *obj = data;
  int ret = luavgl_set_property(L, obj, bar_property_table);

  if (ret == 0) {
    return 0;
  }
  /* a base obj property? */
  ret = luavgl_obj_set_property_kv(L, obj);
  if (ret != 0) {
    debug("unkown property for bar.\n");
  }

  return ret;
}

static int luavgl_bar_set(lua_State *L)
{
  lv_obj_t *obj = luavgl_to_obj(L, 1);

  if (!lua_istable(L, -1)) {
    luaL_error(L, "expect a table on 2nd para.");
    return 0;
  }

  luavgl_iterate(L, -1, luavgl_bar_set_property_kv, obj);

  return 0;
}

static int luavgl_bar_tostring(lua_State *L)
{
  lv_obj_t *obj = luavgl_to_obj(L, 1);
  lua_pushfstring(L, "lv_bar:%p", obj);
  return 1;
}

static const luaL_Reg luavgl_bar_methods[] = {
    {"set",             luavgl_bar_set            },
    {NULL,              NULL                      },
};

static void luavgl_bar_init(lua_State *L)
{
  luavgl_obj_newmetatable(L, &lv_bar_class, "lv_bar", luavgl_bar_methods);
  lua_pushcfunction(L, luavgl_bar_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_pop(L, 1);
}
