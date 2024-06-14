#include "luavgl.h"
#include "private.h"

static char *to_lower(char *str)
{
  for (char *s = str; *s; ++s)
    *s = *s >= 'A' && *s <= 'Z' ? *s | 0x60 : *s;
  return str;
}

static char *luavgl_strchr(const char *s, char c)
{
  while (*s) {
    if (c == *s) {
      return (char *)s;
    }
    s++;
  }
  return NULL;
}

/**
 * Dynamic font family fallback is not supported.
 * The fallback only happen when font creation fails and continue to try next
 * one. Fallback logic in lvgl is supposed to be system wide.
 *
 * lvgl.Font("MiSansW medium, montserrat", 24, "normal")
 */
static int luavgl_font_create(lua_State *L)
{

  if (!lua_isstring(L, 1)) {
    return luaL_argerror(L, 1, "expect string");
  }
  const char *name = lua_tostring(L, 1);
  const lv_font_t *font = NULL;

  luavgl_ctx_t *ctx = luavgl_context(L);
  if (ctx->make_font) {
    font = ctx->make_font(name);
  }

  if (font) {
    lua_pushlightuserdata(L, (void *)font);
    return 1;
  }

  return luaL_error(L, "cannot create font");
}
