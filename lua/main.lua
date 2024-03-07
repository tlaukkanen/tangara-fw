local font = require("font")
local vol = require("volume")
local controls = require("controls")
local time = require("time")

local lock_time = time.ticks()

-- Set up property bindings that are used across every screen.
GLOBAL_BINDINGS = {
  -- Show an alert with the current volume whenver the volume changes.
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
  -- When the device has been locked for a while, default to showing the now
  -- playing screen after unlocking.
  controls.lock_switch:bind(function(locked)
    if locked then
      lock_time = time.ticks()
    elseif time.ticks() - lock_time > 8000 then
      local queue = require("queue")
      if queue.size:get() > 0 then
        require("playing"):pushIfNotShown()
      end
    end
  end),
}

local backstack = require("backstack")
local main_menu = require("main_menu")

backstack.push(main_menu)
