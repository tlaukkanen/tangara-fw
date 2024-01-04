local font = require("font")
local vol = require("volume")

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

local backstack = require("backstack")
local main_menu = require("main_menu")

backstack.push(main_menu)
