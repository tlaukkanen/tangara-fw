#include "luavgl.h"
#include "private.h"
#include <src/widgets/switch/lv_switch.h>

static int luavgl_switch_create(lua_State *L) {
  return luavgl_obj_create_helper(L, lv_switch_create);
}

LUALIB_API int luavgl_switch_set_property_kv(lua_State *L, void *data) {
  lv_obj_t *obj = data;
  /* switches only use base properties */
  int ret = luavgl_obj_set_property_kv(L, obj);
  if (ret != 0) {
    LV_LOG_ERROR("unkown property for switch.\n");
  }

  return ret;
}

static int luavgl_switch_set(lua_State *L) {
  lv_obj_t *obj = luavgl_to_obj(L, 1);

  if (!lua_istable(L, -1)) {
    luaL_error(L, "expect a table on 2nd para.");
    return 0;
  }

  luavgl_iterate(L, -1, luavgl_switch_set_property_kv, obj);

  return 0;
}

static int luavgl_switch_enabled(lua_State *L) {
  lv_obj_t *obj = luavgl_to_obj(L, 1);
  lua_pushboolean(L, lv_obj_has_state(obj, LV_STATE_CHECKED));
  return 1;
}

static int luavgl_switch_tostring(lua_State *L) {
  lv_obj_t *obj = luavgl_to_obj(L, 1);
  lua_pushfstring(L, "lv_switch:%p", obj);
  return 1;
}

static const luaL_Reg luavgl_switch_methods[] = {
    {"set", luavgl_switch_set},
    {"enabled", luavgl_switch_enabled},
    {NULL, NULL},
};

static void luavgl_switch_init(lua_State *L) {
  luavgl_obj_newmetatable(L, &lv_switch_class, "lv_switch",
                          luavgl_switch_methods);
  lua_pushcfunction(L, luavgl_switch_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_pop(L, 1);
}
