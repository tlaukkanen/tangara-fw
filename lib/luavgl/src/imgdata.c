#include "draw/lv_image_decoder.h"
#include "lauxlib.h"
#include "lua.h"
#include "luavgl.h"
#include "misc/lv_color.h"
#include "misc/lv_types.h"
#include "private.h"
#include "stdlib/lv_mem.h"
#include <stdint.h>
#include <string.h>

static int luavgl_imgdata_create(lua_State *L)
{
  if (!lua_isstring(L, 1)) {
    return luaL_argerror(L, 1, "expect string");
  }

  lv_image_decoder_dsc_t descriptor;
  lv_res_t res = lv_image_decoder_open(&descriptor, lua_tostring(L, 1), NULL);
  if (res != LV_RES_OK) {
    return luaL_error(L, "failed to decode image.");
  }

  lv_image_dsc_t *data = lv_malloc(sizeof(lv_image_dsc_t));
  data->header = descriptor.header;
  data->data_size = descriptor.decoded->data_size;

  uint8_t *data_copy = lv_malloc(data->data_size);
  memcpy(data_copy, descriptor.decoded->data, data->data_size);
  data->data = data_copy;

  lv_image_decoder_close(&descriptor);

  lua_pushlightuserdata(L, data);
  return 1;
}
