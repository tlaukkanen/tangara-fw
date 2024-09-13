/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screenshot.hpp"
#include <sys/_stdint.h>

#include <string>

#define LODEPNG_NO_COMPILE_CPP
#include "libs/lodepng/lodepng.h"

#include "esp_log.h"
#include "lvgl.h"

namespace ui {

[[maybe_unused]] static constexpr char kTag[] = "screenshot";

auto SaveScreenshot(lv_obj_t* obj, const std::string& path) -> void {
  lv_draw_buf_t* buf = lv_snapshot_take(obj, LV_COLOR_FORMAT_RGB888);
  if (!buf) {
    return;
  }

  // LVGL appears to output BGR data instead. Not quite sure why, but swapping
  // each pair is quite easy.
  for (size_t i = 0; i < buf->data_size; i += 3) {
    uint8_t temp = buf->data[i];
    buf->data[i] = buf->data[i + 2];
    buf->data[i + 2] = temp;
  }

  // The LVGL lodepng fork uses LVGL's file API, so an extra '/' is needed.
  std::string fullpath = "//sd/" + path;

  auto res = lodepng_encode_file(fullpath.c_str(), buf->data, buf->header.w,
                                 buf->header.h, LCT_RGB, 8);

  lv_draw_buf_destroy(buf);
  if (res != 0) {
    ESP_LOGE(kTag, "lodepng error: '%s'", lodepng_error_text(res));
  }
}

}  // namespace ui
