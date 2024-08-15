local lvgl = require("lvgl")
local font = require("font")

local background_color = "#ffffff"
local background_muted = "#f2f2f2"
local text_color = "#2c2c2c"
local highlight_color = "#ff82bc"
local icon_enabled_color = "#ff82bc"
local icon_disabled_color = "#999999"
local border_color = "#888888"

local theme_light = {
  base = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(0),
      text_font = font.fusion_12,
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
      bg_color = background_muted,
    }},
  },
  pop_up = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_muted,
    }},
  },
  button = {
    {lvgl.PART.MAIN, lvgl.Style {
      pad_left = 2,
      pad_right = 2,
      pad_top = 1,
      pad_bottom = 1,
      bg_color = background_color,
      image_recolor_opa = 180,
      image_recolor = highlight_color,
      radius = 4,
      border_color = border_color,
      border_width = 1,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      text_color = "#ffffff",
      bg_color = highlight_color,
      image_recolor_opa = 0,
    }},
  },
  listbutton = {
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = highlight_color,
      text_color = "#ffffff"
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
      bg_color = background_muted,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      border_color = border_color,
      border_width = 1,
      height = 8,
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = highlight_color,
      border_color = border_color,
      border_width = 1,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_muted,
      border_color = border_color,
      border_width = 1,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = background_muted,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = highlight_color,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = highlight_color,
    }},
  },
  scrubber = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = background_muted,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = highlight_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_muted,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = background_muted,
    }},
    {lvgl.PART.KNOB | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = highlight_color,
      pad_all = 1,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = highlight_color,
    }},
  },
  switch = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      width = 18,
      height = 10,
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
    }},
    {lvgl.PART.INDICATOR, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_color = background_color,
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = highlight_color,
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      bg_opa = lvgl.OPA(100),
    }},
    {lvgl.PART.KNOB | lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = highlight_color,
    }},
  },
  dropdown = {
    {lvgl.PART.MAIN, lvgl.Style{
      radius = 2, 
      pad_all = 2,
      border_width = 1,
      border_color = border_color,
      border_side = 15, -- LV_BORDER_SIDE_FULL
      bg_color = background_color,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      border_color = highlight_color,
    }},
  },
  dropdownlist = {
    {lvgl.PART.MAIN, lvgl.Style{
      radius = 2, 
      pad_all = 2,
      border_width = 1,
      border_color = border_color,
      bg_opa = lvgl.OPA(100),
      bg_color = background_color
    }},
    {lvgl.PART.SELECTED | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = highlight_color,
    }},
  },
  database_indicator = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 180,
      image_recolor = highlight_color,
    }},
  },
  back_button = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 180,
      image_recolor = icon_enabled_color,
      pad_all = 0,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      image_recolor_opa = 0,
      image_recolor = "#ffffff",
    }},
  },
  settings_title = {
   {lvgl.PART.MAIN, lvgl.Style {
      pad_top = 2,
      pad_bottom = 4,
      text_font = font.fusion_10,
      text_color = highlight_color,
    }},
  },
  icon_disabled = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 180,
      image_recolor = icon_disabled_color,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      image_recolor_opa = 0,
      image_recolor = "#ffffff",
    }},
  },
  icon_enabled = {
    {lvgl.PART.MAIN, lvgl.Style {
      image_recolor_opa = 180,
      image_recolor = icon_enabled_color,
    }},
    {lvgl.PART.MAIN | lvgl.STATE.FOCUSED, lvgl.Style {
      image_recolor_opa = 0,
      image_recolor = "#ffffff",
    }},
  },
  now_playing = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      border_width = 1,
      border_color = border_color,
      border_side = 15, -- LV_BORDER_SIDE_FULL
    }},
  },
  menu_icon = {
    {lvgl.PART.MAIN, lvgl.Style {
      pad_all = 4,
      radius = 4
    }},
  },
}

return theme_light
