local lvgl = require("lvgl")
local font = require("font")

local theme = {
  list_item = lvgl.Style {
    pad_left = 4,
    pad_right = 4,
  },
  list_heading = lvgl.Style {
    pad_top = 4,
    pad_left = 4,
    pad_right = 4,
    text_font = font.fusion_10,
    text_align = lvgl.ALIGN.CENTER,
  },
  settings_title = lvgl.Style {
    pad_top = 2,
    pad_bottom = 4,
    text_font = font.fusion_10,
  }
}

return theme
