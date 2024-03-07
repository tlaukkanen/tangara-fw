local font = require("font")
local vol = require("volume")
local theme = require("theme")

-- Set up property bindings that are used across every screen.
GLOBAL_BINDINGS = {
  vol.current_pct:bind(function(pct)
    require("alerts").show(function()
      local container = lvgl.Object(nil, {
        w = lvgl.PCT(80),
        h = lvgl.SIZE_CONTENT,
        flex = {
          flex_direction = "column",
          justify_content = "center",
          align_items = "center",
          align_content = "center",
        },
        bg_opa = lvgl.OPA(100),
        bg_color = "#fafafa",
        radius = 8,
        pad_all = 2,
      })
      container:Label {
        text = string.format("Volume %i%%", pct),
        text_font = font.fusion_10
      }
      container:Bar {
        w = lvgl.PCT(100),
        h = 8,
        range = { min = 0, max = 100 },
        value = pct,
      }
      container:center()
    end)
  end),
}

local lvgl = require("lvgl")
local my_theme = {
  base = {
    {lvgl.PART.MAIN, lvgl.Style {
      bg_opa = lvgl.OPA(0),
      text_font = font.fusion_12,
      text_color = "#000000",
    }},
    {lvgl.STATE.FOCUSED, lvgl.Style {
      bg_opa = lvgl.OPA(100),
      bg_color = "#E3F2FD",
    }},
  },
  button = {
    {lvgl.PART.MAIN, lvgl.Style {
      pad_left = 2,
      pad_right = 2,
      pad_top = 1,
      pad_bottom = 1,
      bg_color = "#ffffff",
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
      bg_color = "#2196F3",
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      pad_all = 2,
      bg_color = "#ffffff",
      shadow_width = 5,
      shadow_opa = lvgl.OPA(100)
    }},
    {lvgl.STATE.FOCUSED, lvgl.Style {
      bg_color = "#BBDEFB",
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
      bg_color = "#9E9E9E",
    }},
    {lvgl.PART.INDICATOR | lvgl.STATE.CHECKED, lvgl.Style {
      bg_color = "#2196F3",
    }},
    {lvgl.PART.KNOB, lvgl.Style {
      radius = 32767, -- LV_RADIUS_CIRCLE = 0x7fff
      pad_all = 2,
      bg_opa = lvgl.OPA(100),
      bg_color = "#ffffff",
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
      bg_color = "#ffffff"
    }}
  }
}
theme.set(my_theme)

local backstack = require("backstack")
local main_menu = require("main_menu")

backstack.push(main_menu)
