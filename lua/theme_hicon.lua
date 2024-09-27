local lvgl = require("lvgl")
local font = require("font")

-- local background_color = "#000000"
-- local text_color = "#33b5e5"

local text_color = "#000000"
local background_color = "#FFFFFF"

local theme_hicon = {
  base = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(0),
      text_font = font.fusion_12,
    }},
    {lvgl.PART.SCROLLBAR, lvgl.Style {
      bg_color = text_color,
    }},
  },
  root = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color, -- Root background color
      text_color = text_color
    }},
  },
  header = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
    }},
  },
  pop_up = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
      border_color = text_color,
      border_width = 1,
    }},
  },
  button = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      pad_left = 2,
      pad_right = 2,
      pad_top = 1,
      pad_bottom = 1,
      bg_color = background_color,
      image_recolor_opa = 255,
      image_recolor = text_color,
      radius = 5,
      border_color = text_color,
      border_width = 1,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = text_color,
      image_recolor_opa = 255,
      image_recolor = background_color,
      text_color = background_color,
    }},
  },
  listbutton = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
      flex_cross_place = lvgl.FLEX_ALIGN.CENTER,
      pad_column = 4,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = text_color,
      text_color = background_color,
      image_recolor_opa = 255,
      image_recolor = background_color,
    }},
  },
  bar = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
      border_color = text_color,
      border_width = 1,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = text_color,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
  },
  slider = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      border_color = text_color,
      border_width = 1,
      height = 8,
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = text_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_color,
      border_color = text_color,
      border_width = 1,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = background_color,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = text_color,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.EDITED, lvgl.Style {
      pad_all = 2,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = text_color,
    }},
  },
  scrubber = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      border_color = text_color,
      border_width = 1,
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = text_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_color,
      border_color = text_color,
      border_width = 1,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = background_color,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = text_color,
      pad_all = 1,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.EDITED, lvgl.Style {
      pad_all = 4,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = text_color,
    }},
  },
  switch = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      width = 18,
      height = 10,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_color,
      border_color = text_color,
      border_width = 1,
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_color,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = text_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
      border_color = text_color,
      border_width = 1,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = text_color,
    }},
  },
  dropdown = {
    {lvgl.PART.MAIN, lvgl.Style{
      radius = 2, 
      pad_all = 2,
      border_width = 1,
      border_color = text_color,
      border_side = 15, -- LV_BORDER_SIDE_FULL
      bg_color = background_color,
      bg_opa = lvgl.OPA(100),
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      border_color = text_color,
      bg_color = text_color,
      text_color = background_color,
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.FOCUSED, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = background_color,
    }},
  },
  dropdownlist = {
    {lvgl.PART.MAIN, lvgl.Style{
      radius = 2, 
      pad_all = 2,
      border_width = 1,
      border_color = text_color,
      bg_opa = lvgl.OPA(100),
      bg_color = background_color
    }},
    {lvgl.PART.SELECTED | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = text_color,
      text_color = background_color,
    }},
  },
  database_indicator = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  settings_title = {
   {lvgl.PART.MAIN, lvgl.Style {
      pad_top = 2,
      pad_bottom = 4,
      text_font = font.fusion_10,
      text_color = text_color,
    }},
  },
  icon_disabled = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  icon_enabled = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  now_playing = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      border_width = 1,
      border_color = text_color,
      border_side = 15, -- LV_BORDER_SIDE_FULL
    }},
  },
  menu_icon = {
    {lvgl.PART.MAIN, lvgl.Style {
      pad_all = 4,
      radius = 4
    }},
  },
  bluetooth_icon = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  battery = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  battery_0 = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  battery_charging = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
  battery_charge_icon = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 200,
      image_recolor = text_color,
    }},
  },
  battery_charge_outline = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = background_color,
    }},
  },
  regulatory_icons = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 255,
      image_recolor = text_color,
    }},
  },
}

return theme_hicon
