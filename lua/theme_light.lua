local lvgl = require("lvgl")
local font = require("font")

local background_color = "#FFFFFF"
local background_muted = "#FFFFFF"
local text_color = "#000000"
local highlight_color = "#E3F2FD"

local theme_light = {
  base = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(0),
      bg_color = background_color, -- Root background color
      text_font = font.fusion_12,
      text_color = text_color,
    }},
    {lvgl.STATE.FOCUSED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = highlight_color,
    }},
  },
  root = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_color, -- Root background color
    }},
  },
  button = {
    {lvgl.PART.MAIN, lvgl.Style {
      pad_left = 2,
      pad_right = 2,
      pad_top = 1,
      pad_bottom = 1,
      bg_color = background_color,
      radius = 5,
    }},
  },
  bar = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
  },
  slider = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = highlight_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      pad_all = 2,
      bg_color = background_color,
      shadow_width = 5,
      shadow_opa = lvgl.OPA(100)
    }},
    {lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = highlight_color,
    }},
  },
  switch = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      width = 28,
      height = 18,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_muted,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = highlight_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      pad_all = 2,
      bg_opa = lvgl.OPA(100),
      bg_color = background_color,
    }},
  },
  dropdown = {
    {lvgl.PART.MAIN, lvgl.Style{
      radius = 2, 
      pad_all = 2,
      border_width = 1,
      border_color = "#2196F3",
      border_side = 15, -- LV_BORDER_SIDE_FULL
    }}
  },
  dropdownlist = {
    {lvgl.PART.MAIN, lvgl.Style{
      radius = 2, 
      pad_all = 2,
      border_width = 1,
      border_color = "#607D8B",
      bg_opa = lvgl.OPA(100),
      bg_color = background_color
    }}
  }
}

return theme_light
