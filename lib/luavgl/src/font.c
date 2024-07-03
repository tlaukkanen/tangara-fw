#include "luavgl.h"
#include "private.h"

static int luavgl_font_create(lua_State *L)
{
  if (!lua_isstring(L, 1)) {
    return luaL_argerror(L, 1, "expect string");
  }
  if (!lua_isfunction(L, 2)) {
    return luaL_argerror(L, 1, "expect function");
  }

  luavgl_ctx_t *ctx = luavgl_context(L);
  if (!ctx->make_font) {
    return luaL_error(L, "cannot create font");
  }

  const char *name = lua_tostring(L, 1);
  int cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ctx->make_font(L, name, cb_ref);

  return 0;
}
