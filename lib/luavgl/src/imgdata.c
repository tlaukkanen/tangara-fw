#include "draw/lv_img_buf.h"
#include "draw/lv_img_decoder.h"
#include "lauxlib.h"
#include "lua.h"
#include "luavgl.h"
#include "misc/lv_color.h"
#include "misc/lv_mem.h"
#include "misc/lv_types.h"
#include "private.h"
#include <stdint.h>
#include <string.h>

static int luavgl_imgdata_create(lua_State *L)
{
  if (!lua_isstring(L, 1)) {
    return luaL_argerror(L, 1, "expect string");
  }

  lv_img_decoder_dsc_t descriptor;
  lv_res_t res =
      lv_img_decoder_open(&descriptor, lua_tostring(L, 1), lv_color_black(), 0);
  if (res != LV_RES_OK) {
    return luaL_error(L, "failed to decode image.");
  }

  lv_img_dsc_t *data = lv_mem_alloc(sizeof(lv_img_dsc_t));
  data->header = descriptor.header;
  data->data_size = data->header.w * data->header.h * sizeof(uint32_t); // ???

  uint8_t *data_copy = lv_mem_alloc(data->data_size);
  memcpy(data_copy, descriptor.img_data, data->data_size);
  data->data = data_copy;

  lv_img_decoder_close(&descriptor);

  lua_pushlightuserdata(L, data);
  return 1;
}
