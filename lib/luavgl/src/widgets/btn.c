#include "luavgl.h"
#include "private.h"

static int luavgl_btn_create(lua_State *L)
{
  return luavgl_obj_create_helper(L, lv_btn_create);
}

LUALIB_API int luavgl_btn_set_property_kv(lua_State *L, void *data)
{
  lv_obj_t *obj = data;
  /* a base obj property? */
  int ret = luavgl_obj_set_property_kv(L, obj);
  if (ret != 0) {
    LV_LOG_ERROR("unkown property for btn.\n");
  }

  return ret;
}

static int luavgl_btn_set(lua_State *L)
{
  lv_obj_t *obj = luavgl_to_obj(L, 1);

  if (!lua_istable(L, -1)) {
    luaL_error(L, "expect a table on 2nd para.");
    return 0;
  }

  luavgl_iterate(L, -1, luavgl_btn_set_property_kv, obj);

  return 0;
}

static int luavgl_btn_tostring(lua_State *L)
{
  lv_obj_t *obj = luavgl_to_obj(L, 1);
  lua_pushfstring(L, "lv_btn:%p", obj);
  return 1;
}

static const luaL_Reg luavgl_btn_methods[] = {
    {"set",             luavgl_btn_set            },
    {NULL,              NULL                      },
};

static void luavgl_btn_init(lua_State *L)
{
  luavgl_obj_newmetatable(L, &lv_button_class, "lv_btn", luavgl_btn_methods);
  lua_pushcfunction(L, luavgl_btn_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_pop(L, 1);
}
